#ifndef AUDIOBUFFERSPSC_H_
#define AUDIOBUFFERSPSC_H_
// tha idea

// use two structs to keep track of a "minimum guaranteed" access size

// reference: http://daugaard.org/blog/writing-a-fast-and-versatile-spsc-ring-buffer/

// spsc:
//  - on their own, the producer and consumer should have the resources necessary
//    to read/write freely to some allowable extent.
//
//  - when our reader runs dry, we want to check a locked structure to verify its data.
//    then continue on as normal.

// for the callback, need a prealloc'd structure to read out to
// also need some exterior "data" parameter where we can put that shit so that
// this does necessitate a read "lock" so that we can get the data out in one piece
// but the procedure should be quick

// alternative:
// if the other thread has stale data, could we justre bake in a fallback method?
// i.e. attempt to fetch from the read buffer, then just pop the data if not possible?

#include <atomic>
#include <cstdint>
#include <cmath>

#include <iostream>

struct pc_marker {
  pc_marker() : position(0), safesize(0) { }
  uint32_t position; // the index which the marker points to (masked 2x)
  uint32_t safesize;  // minimum number of elements which are guaranteed to be available
};

template <typename BUFFER_UNIT>
class AudioBufferSPSC {
 public:
  AudioBufferSPSC(int twopow) :
    buffer_capacity_(pow(2, twopow)),
    buffer_(new BUFFER_UNIT[buffer_capacity_]),
    shared_read_(0),
    shared_write_(0),
    reader_thread_(pc_marker()),
    writer_thread_(pc_marker()),
    read_marker_(0),
    readzone_(new BUFFER_UNIT[buffer_capacity_])
    {
      writer_thread_.safesize = buffer_capacity_;
    }

  /**
   * Reads a portion of the buffer (if possible) to the readzone, and
   * returns a pointer to it. Does not advance the read-marker.
   * 
   * Arguments:
   *  count, the number of elements to be read.
   *  output, an output parameter which will point to the desired memory.
   * 
   * Returns:
   *  The number of bytes which could be read.
   */ 
  size_t Peek(uint32_t count, BUFFER_UNIT** output) {
    // TODO: output param is inconsistent
    //       might be ideal? depending on use cases but ich dont think so
    uint32_t len;
    if (reader_thread_.safesize < count) {
      // no guarantee that we have enough room to read -- check the atomic
      UpdateReaderThread();
    }

    if (reader_thread_.safesize < count) {
      // cannot read all bits
      len = reader_thread_.safesize;
    } else {
      // can read all bits
      len = count;
    }

    // nothing to do :)
    if (len == 0) {
      return 0;
    }
    
    uint32_t masked_read = Mask(reader_thread_.position);
    for (int i = 0; i < len; i++) {
      if (masked_read >= buffer_capacity_) {
        masked_read -= buffer_capacity_;
      }

      readzone_[i] = buffer_[masked_read++];
    }

    *output = readzone_;
    return len;
  }

  bool Skip(uint32_t count) {
    if (reader_thread_.safesize < count) {
      UpdateReaderThread();
    }

    if (reader_thread_.safesize < count) {
      return false;
    }

    reader_thread_.position = MaskTwo(reader_thread_.position + count);
    reader_thread_.safesize -= count;

    shared_read_.store(reader_thread_.position, std::memory_order_release);
    read_marker_.fetch_add(count, std::memory_order_acq_rel);
  }

  /**
   * Attempt to read from the buffer and advance the cursor.
   * Returns a pointer if the memory requested exists, otherwise
   * returns a nullptr.
   * 
   * Arguments:
   *  - count, the number of elements we are attempting to read.
   * 
   * Returns:
   *  - a R/W pointer to the read data.
   */ 
  BUFFER_UNIT* Read(uint32_t count) {
    // check safesize
    if (reader_thread_.safesize < count) {
      UpdateReaderThread();

      if (reader_thread_.safesize < count) {
        return nullptr; 
      }
    }

    // ample space
    uint32_t masked_read = Mask(reader_thread_.position);
    for (int i = 0; i < count; i++) {
      if (masked_read >= buffer_capacity_) {
        masked_read -= buffer_capacity_;
      }
      readzone_[i] = buffer_[masked_read++];
    }

    reader_thread_.position = MaskTwo(reader_thread_.position + count);
    reader_thread_.safesize -= count;

    // update read atomic
    shared_read_.store(reader_thread_.position, std::memory_order_release);
    read_marker_.fetch_add(count, std::memory_order_acq_rel);

    return readzone_;
  }

  /**
   * A more flexible version of the read call which reads at most the number of items specified
   * and passes through the queue accordingly.
   * 
   * Arguments:
   *  - count, the maximum number of elements we want to read.
   *  - items_read, an output parameter which will contain the number of items read.
   * 
   * Returns:
   *  - A pointer to an array of queue entries containing the number of items specified
   *    by items_read.
   */ 
  BUFFER_UNIT* ReadMaximum(uint32_t count, int* items_read) {
    if (reader_thread_.safesize < count) {
      UpdateReaderThread();
    }
    
    BUFFER_UNIT* retval = Read(Min(count, reader_thread_.safesize));
    *items_read = count;

    return retval;
  }

  /**
   *  Synchronizes the audio buffer to a given sample number.
   *  Note: buffer is emptied if sample_num is larget than the number of entries currently contained
   */ 
  void Synchronize(uint32_t sample_num) {
    int count = sample_num - read_marker_.load(std::memory_order_acquire);
    // ensure that we have enough space to leap
    if (count > 0) {
      if (count > reader_thread_.safesize) {
        UpdateReaderThread();
      }

      // wipes the buffer if necessary
      int offset = Min(reader_thread_.safesize, count);
      // return whether or not the buffer has been emptied


      reader_thread_.safesize -= offset;
      reader_thread_.position = MaskTwo(reader_thread_.position + offset);
      shared_read_.store(reader_thread_.position, std::memory_order_release);
      read_marker_.fetch_add(count, std::memory_order_release);
    }
  }

  /**
   * Write to the buffer.
   * Returns whether or not the write was successful --
   * false if not enough room, true otherwise.
   */ 
  bool Write(const BUFFER_UNIT* data, uint32_t count) {
    if (writer_thread_.safesize < count) {
      UpdateWriterThread();
    }

    if (writer_thread_.safesize < count) {
      return false;
    }

    uint32_t masked_write = Mask(writer_thread_.position);
    for (int i = 0; i < count; i++) {
      if (masked_write >= buffer_capacity_) {
        masked_write -= buffer_capacity_;
      }
      buffer_[masked_write++] = data[i];
    }

    writer_thread_.position = MaskTwo(writer_thread_.position + count);
    writer_thread_.safesize -= count;

    shared_write_.store(writer_thread_.position, std::memory_order_release);

    return true;
  }

  /**
   *  Wipes the contents of the queue. Not thread safe.
   */ 
  void Clear() {
    writer_thread_.position = 0;
    reader_thread_.position = 0;
    writer_thread_.safesize = buffer_capacity_;
    reader_thread_.safesize = 0;
    shared_read_.store(0);    // not worried about perofrmant memory order
    shared_write_.store(0);
    read_marker_.store(0);    // wipe history as well
  }

  // functions to add:
    // ForceWrite, which will guarantee that data is written again

  uint32_t GetMaximumWriteSize() {
    UpdateWriterThread();
    return writer_thread_.safesize;
  }

  uint32_t Size() const {
    uint32_t size = shared_write_.load(std::memory_order_acquire);
    size -= shared_read_.load(std::memory_order_acquire);
    return MaskInclusive(size);
  }

  uint32_t Empty() const {
    // eesh
    uint32_t write = shared_write_.load(std::memory_order_acquire);
    return (write == shared_read_.load(std::memory_order_acquire));
  }

  uint32_t Capacity() const {
    return buffer_capacity_;
  }

  uint32_t GetItemsRead() const {
    return read_marker_.load(std::memory_order_acq_rel);
  }

  ~AudioBufferSPSC() {
    delete[] buffer_;
    delete[] readzone_;
  }

  // implement an assign op which populates the local buffer
  // with contents of another
  // (so that synchro call can repop if necessary)

  // the best we have in this case is a skip operation
  void operator=(const AudioBufferSPSC& other) = delete;
  AudioBufferSPSC(const AudioBufferSPSC &) = delete;

 private:
  const uint32_t buffer_capacity_;  // max capacity of the buffer
  BUFFER_UNIT* buffer_;   // pointer to internal buffer

  pc_marker reader_thread_;
  std::atomic_uint32_t shared_read_;  // shared ptr for syncing read val
  std::atomic_uint64_t read_marker_;  // tracks number of samples read thus far
  
  char CACHE_BUSTER[64];  // yall is playn -_-

  pc_marker writer_thread_;
  std::atomic_uint32_t shared_write_;   // shared ptr for syncing write val

  // todo: resolve issue of reading data while it may be written at the same time
  // in the read thread: read to a second ring buffer which can be shared with a render thread.
  // even better: if the goal is an fft printout, we can rely on an atomic int to track sample count,
  // and read a set size from that ring buffer, only advancing when necessary

  // a safe readzone we can use to avoid memory allocation
  // not limited by pow2 restriction!
  BUFFER_UNIT* readzone_;   // read space which can be read/modified by read thread

  /**
   * Mask a given value based on the length of the buffer.
   * 
   * Arguments:
   *  - input, the value which we are masking.
   * 
   * Returns:
   *  - The inputted argument, masked to the width of the buffer capacity.
   */ 
  uint32_t Mask(uint32_t input) const {
    return input & (buffer_capacity_ - 1);
  }

  uint32_t MaskInclusive(uint32_t input) const {
    uint32_t masked_input = Mask(input);
    if (input != 0 && masked_input == 0) {
      // multiple of capacity
      return buffer_capacity_;
    }

    return masked_input;
  }

  /**
   * Mask the input to a range of 2 * the buffer length.
   * 
   * Arguments:
   *  - input, the value we are masking.
   * 
   * Returns:
   *  - masked value
   */ 
  uint32_t MaskTwo(uint32_t input) const {
    return input & ((buffer_capacity_ << 1) - 1);
  }

  // updates the reader thread's safe size
  void UpdateReaderThread() {
    uint32_t pos = shared_write_.load(std::memory_order_acquire);
    reader_thread_.safesize = MaskInclusive(pos - reader_thread_.position);
  }

  // updates the writer thread's safe size
  void UpdateWriterThread() {
    uint32_t pos = shared_read_.load(std::memory_order_acquire);
    writer_thread_.safesize = buffer_capacity_ - MaskInclusive(writer_thread_.position - pos);
  }

  /**
   * Shorthand for min
   */ 
  inline uint32_t Min(uint32_t a, uint32_t b) {
    return (a < b ? a : b);
  }
};  // class AudioBufferSPSC

#endif  // AUDIOBUFFERSPSC_H_