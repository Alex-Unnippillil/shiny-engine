// SPDX-License-Identifier: MIT
#pragma once
#include "engine.hpp"
#include "../nr-worker/protocol.hpp"
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
namespace shiny::player {
class NrProcess {
 HANDLE process=nullptr,thread=nullptr,job=nullptr,input=nullptr,output=nullptr;
 public:
 NrProcess(const std::wstring& args);
 ~NrProcess();
 NrProcess(const NrProcess&)=delete;NrProcess& operator=(const NrProcess&)=delete;
 void cancel();
 void send(const void*,size_t);
 void receive(void*,size_t,std::stop_token,unsigned timeoutMs=30000);
 std::string readText(std::stop_token,unsigned timeoutMs=30000);
 bool succeeded()const;
};
struct NrImage {nrwire::Header header;std::vector<uint8_t> original,enhanced;double milliseconds=0;uint64_t controlRevision=0;int64_t sourceTimeMs=0;};
class NrSession {
 std::mutex mutex;std::condition_variable_any changed;std::jthread thread;
 std::shared_ptr<NrProcess> process;std::optional<NrImage> pending,complete;
 std::string statusText="Not prepared";bool isReady=false,done=false;uint32_t serial=0;
 void status(std::string text,bool ready=false);
 public:
 explicit NrSession(const std::filesystem::path& model, std::string researchDigest={});
 ~NrSession();
 void submit(NrImage frame);
 std::optional<NrImage> take();
 std::string status();bool ready();bool finished();
};
// A separate muted decoder, not synchronized to main-player audio.
class NrSource {
 struct Pixels;
 std::unique_ptr<Pixels> pixels;std::unique_ptr<Engine> engine;
 public:
 NrSource(std::shared_ptr<VlcApi>,const Item&,uint32_t width,uint32_t height,int64_t time);
 ~NrSource();
 std::optional<NrImage> sample();
 void pause(bool);void seek(int64_t);int64_t time()const;bool playing()const;bool seekable()const;
};
class NeuralPanel {
 struct Impl;std::unique_ptr<Impl> impl;
 public:
 NeuralPanel(HWND,std::shared_ptr<VlcApi>,const Item&,uint32_t,uint32_t,int64_t);
 ~NeuralPanel();bool visible()const;
};
}
