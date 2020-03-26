
// fuck!my hi.freq tonal alarms are going off the walls...

// provide interface for communicating back+forth with stb vorbis 
#ifndef VORBIS_MANAGER_H_
#define VORBIS_MANAGER_H_
#include "audiohandlers/AudioBufferSPSC.hpp"
#include "vorbis/stb_vorbis.h"
#include "portaudio.h"

#include <memory>
#include <list>
#include <mutex>
#include <thread>
#include <chrono>
#include <functional>

// a lot of this stuff is provided by the lib already
// but the aim is to just make some c calls into cpp calls

/**
 *  A TimeInfo provides some stats on the currently-running thread.
 *  The idea is that this should allow a client to synchronize themselves
 *  with the thread currently being read.
 */ 
struct TimeInfo {
 public:
  /**
   * Construct a TimeInfo
   * 
   * Default: Sample rate of 0, epoch is the unix epoch.
   * Const: Sample rate and epoch are set based on currently running file.
   */ 
  TimeInfo() : sample_rate_(0), playback_epoch_() { }
  TimeInfo(std::chrono::time_point<std::chrono::high_resolution_clock> epoch, 
           int sample_rate) : sample_rate_(sample_rate), playback_epoch_(epoch) { }

  void operator=(const TimeInfo& info) {
    sample_offset_.store(info.sample_offset_, std::memory_order_release);
    sample_rate_ = info.sample_rate_;
    playback_epoch_ = info.playback_epoch_;
  };

  /**
   * Estimates which sample we should be reading from
   */ 
  int GetCurrentSample() const;

  void Reset() {
    sample_offset_ = GetCurrentSample();
  }

  /**
   * Determine whether the TimeInfo is currently valid
   */ 
  bool IsValid() const { return (sample_rate_); }
 private:
  std::chrono::time_point<std::chrono::high_resolution_clock> playback_epoch_;
  // playback start point
  // not sure what this does yet! but im sure it will be cool :)

  // what if the epoch remains constant, and we just modify the offset on a reset call?
  std::atomic<int> sample_offset_;
  int sample_rate_;
};

class VorbisManager {
  // floats only!

  // allows the user to request additional buffers as needed
  // loads content into these buffers as required
  // synchronizes buffers as needed

  // should be constructed with a file object generated from a vorbis stream
 public:
  /**
   * Create a vorbis manager which reads from a given OGG file.
   * Sets up a critical buffer and prepares itself to begin reading into it.
   * 
   * Args:
   *  - twopow, the log_2 of the size of the desired buffer. Should keep as small as possible
   */  
  VorbisManager(int twopow);

  /**
   *  Assigns a file to the vorbis manager
   */ 
  bool SetFilename(char* filename);
  
  /**
   *  Constructs a new audio buffer on the heap which will receive
   *  input from the manager, returning a shared pointer to it.
   * 
   *  All buffers created must be larger than the critical buffer.
   *  If client attempts to create a buffer which is smaller than the critical buffer,
   *  returns nullptr.
   * 
   *  Must be freed by the user.
   */ 
  std::shared_ptr<AudioBufferSPSC<float>> CreateBufferInstance(int twopow);

  /**
   *  Starts the write thread.
   */ 
  bool StartWriteThread();

  /**
   *  Stops the write thread.
   */ 
  bool StopWriteThread();

  /**
   *  If a thread is running, returns a copy of the TimeInfo associated with the current file.
   */ 
  const TimeInfo* GetTimeInfo();

  VorbisManager(VorbisManager&) = delete;
  void operator=(const VorbisManager& m) = delete;
  ~VorbisManager();

 private:
  std::list<std::weak_ptr<AudioBufferSPSC<float>>> buffer_list_;
  AudioBufferSPSC<float>* critical_buffer_;
  uint32_t critical_buffer_capacity_;
  stb_vorbis* audiofile_;

  TimeInfo time_info;

  // some const fields
  unsigned int sample_rate_;
  int channel_count_;

  // vars associated with thread
  std::thread write_thread_;  // thread itself
  std::atomic<bool> run_thread_;  // used to communicate whether the thread should continue running

  float* channel_buffers_;

  // lock for buffer list
  std::mutex buffer_list_lock;

  /**
   *  The function which will populate our buffers when called
   *  WriteThreadFn will perform some check to see if we can call this
   * 
   *  Populates from channel_buffers_. This must be populated prior.
   * 
   *  write_size : the number of elements we are writing
   */ 
  bool PopulateBuffers(uint32_t write_size);

  /**
   *  Function called by our write thread
   *  For now: all buffers are populated by the same thread
   *  But if performance is an issue that can be rearranged (it probably wont be)
   *  Idea: ensure 64 byte size on individual audio queues?
   */ 
  void WriteThreadFn();

  /**
   *  Used internally to handle pretty much any case in which we need to
   *  transform our list of buffers.
   * 
   *  Iterates through all buffers available, checking if they are still valid.
   *  If they are, run the callback.
   *  If not, erase them.
   */ 
  void EraseOrCallback(std::function<void(AudioBufferSPSC<float>*)>);

  /**
   *  Callback function passed to PortAudio
   */
  static int PaCallback( const void* inputBuffer,
                          void* outputBuffer,
                          unsigned long frameCount,
                          const PaStreamCallbackTimeInfo* timeInfo,
                          PaStreamCallbackFlags statusFlags,
                          void* userData  );
};  // class VorbisManager

// The AudioBuffer provides a convenient, low-footprint read/write structure
// which minimizes read footprint

// however, we've now run into the issue of synchronizing activity
// between the fft threads

// we can control the rate at which the buffer is read, but we also need to prevent them
// from desynchronizing

// how would i do it?

// audiobuffer stores decoded data from our file on disk
// if the buffers are completely full, there will be some event in which one buffer
// can be read to but not the other.

// why not make the playback buffer smaller than the other buffers?
// then, whenever we can successfully write to the playback buffer,
// we also write to the remaining buffers.

// the playback buffer is the most crucial, so we'll fill by it.
// since our other buffers are bound to be read at roughly the same rate,
// we'll empty them roughly accordingly, but add some buffer space in case it gets ahead or behind.

#endif  // VORBIS_MANAGER_H_