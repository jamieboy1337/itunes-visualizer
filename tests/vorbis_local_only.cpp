
#include "audiohandlers/VorbisManager.hpp"
#include "audiohandlers/AudioBufferSPSC.hpp"
#include "gtest/gtest.h"
#include "audiohandlers/DFT.hpp"

#include <string>

// read a vorbis file
// close it
// give a VM the same file
// ensure that it is read correctly via a separate thread (GetBuffer)
// give the buffer to the stub code to verify that the input is correct
// and also that it does not terminate early

const std::string testfile = "resources/flap_jack_scream.ogg";

/**
 *  Some assurances on the basic state of a newly constructed manager
 */ 
TEST(VorbisManagerTests, CreateManager) {
  int err = Pa_Initialize();
  std::cout << Pa_GetDeviceCount() << std::endl;
  ASSERT_EQ(err, paNoError);
  VorbisManager* mgr = VorbisManager::GetVorbisManager(15, testfile);
  ASSERT_NE(mgr, nullptr);
  ASSERT_FALSE(mgr->IsThreadRunning());

  ReadOnlyBuffer* buf = mgr->CreateBufferInstance();
  float** res;
  ASSERT_EQ(buf->Peek_Chunked(1024, &res), 0);

  delete buf;
  delete mgr;
  Pa_Terminate();
}

void ReadStream(float* check, ReadOnlyBuffer* buf) {
  int offset = 0;
  size_t samples_read;
  float** output;
  do {
    offset = buf->Synchronize_Chunked() * 2;
    samples_read = buf->Peek_Chunked(2048, &output);
    std::cout << output[0][4] << std::endl;
    // note: left on 0, right on 1
    for (size_t i = 0; i < samples_read; i += 64) {
      
      if (check[offset + (2 * i)] != output[0][i]) {
        std::cout << "bad match: frame " << (offset/2 + i) << "L" << std::endl;
        std::cout << "on frame " << i << std::endl;

        for (int j = 0; j < 32; j++) {
          if (output[0][i] == check[offset + (2 * i) + j - 16]) {
            std::cout << "l-desync" << std::endl;
            std::cout << j << std::endl;
          }
          if (output[1][i] == check[offset + (2 * i) - j - 16]) {
            std::cout << "r-desync" << std::endl;
            std::cout << j << std::endl;
          }
        }

        std::cout << check[offset + (2 * i)] << " expected " << output[0][i] << "actual" << std::endl;
        ASSERT_NEAR(output[0][i], check[offset + (2 * i)], 0.001);
        break;
      }

      if (check[offset + (2 * i) + 1] != output[1][i]) {
        std::cout << "bad match: frame " << (offset/2 + i) << "R" << std::endl;
        std::cout << "on frame " << i << std::endl;
        
        for (int j = 0; j < 32; j++) {
          if (output[0][i] == check[offset + (2 * i) + j - 16]) {
            std::cout << "l-desync" << std::endl;
            std::cout << j << std::endl;
          }
          if (output[1][i] == check[offset + (2 * i) - j]) {
            std::cout << "r-desync" << std::endl;
            std::cout << j << std::endl;
          }
        }

        ASSERT_NEAR(output[1][i], check[offset + (2 * i) + 1], 0.001);
        break;
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(15));
  } while (samples_read != 0);
  std::cout << "done\n\n\n" << std::endl;
}

TEST(VorbisManagerTests, ShawtyWannaFuck) {
  int err;
  stb_vorbis* vorbis = stb_vorbis_open_filename(testfile.c_str(), &err, NULL);
  int filelen = stb_vorbis_stream_length_in_samples(vorbis);
  float* buffer = new float[filelen * 2];
  stb_vorbis_get_samples_float_interleaved(vorbis, 2, buffer, filelen * 2);
  stb_vorbis_close(vorbis);
  
  err = Pa_Initialize();
  ASSERT_EQ(err, paNoError);
  VorbisManager* mgr = VorbisManager::GetVorbisManager(14, testfile);
  ASSERT_NE(mgr, nullptr);
  ReadOnlyBuffer* buf = mgr->CreateBufferInstance();
  ReadOnlyBuffer* buf_two = mgr->CreateBufferInstance();
  ReadOnlyBuffer* buf_three = mgr->CreateBufferInstance();
  ReadOnlyBuffer* buf_four = mgr->CreateBufferInstance();
  #ifdef STUBONLY
    Set_Filename(testfile);
  #endif
  std::cout << "here" << std::endl;
  mgr->StartWriteThread();
  std::cout << "thread started" << std::endl;
  ASSERT_TRUE(mgr->IsThreadRunning());
  std::thread checker(ReadStream, buffer, buf);
  std::thread checker2(ReadStream, buffer, buf_two);
  std::thread checker3(ReadStream, buffer, buf_three);
  std::cout << "start sleep" << std::endl;
  mgr->ThreadWait();

  // an issue arises with verifying this after a call to ForceWrite -- each buffer is adjusted independently
  // by some unknown amount, so we cannot determine where exactly in the file it is reading from.
  // this will not cause issues with PortAudio, only with the stub + file verification

  std::cout << "stopping the write thread..." << std::endl;
  mgr->StopWriteThread();

  checker.join();
  checker2.join();
  checker3.join();
  delete[] buffer;

  delete buf;
  delete buf_two;
  delete buf_three;
  delete buf_four;
  delete mgr;
}

// todo: delete a buffer partway through the thread