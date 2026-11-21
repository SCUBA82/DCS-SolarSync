#pragma once

#include <Arduino.h>
#include <WebServer.h>
#include <functional>
#include "BRMesh.h"
#include "SceneConfig.h"

class WebControl {
 public:
  using LiveDataCallback = std::function<String()>;
  using DateGetCallback = std::function<String()>;
  using DateSetCallback = std::function<bool(int, int, int, String &)>;
  using TimeZoneGetCallback = std::function<String()>;
  using TimeZoneSetCallback = std::function<bool(int, int, int, String &)>;
  using AutoResumeCallback = std::function<void()>;

  explicit WebControl(BRMesh &mesh, SceneConfig &scene, uint16_t port = 80);
  void begin();
  void loop();
  void setHostname(const String &hostname) { hostname_ = hostname; }
  bool isManual() const { return manual_; }
  void onLiveData(LiveDataCallback cb) { liveCallback_ = cb; }
  void onDateGet(DateGetCallback cb) { dateGetCallback_ = cb; }
  void onDateSet(DateSetCallback cb) { dateSetCallback_ = cb; }
  void onTimeZoneGet(TimeZoneGetCallback cb) { timeZoneGetCallback_ = cb; }
  void onTimeZoneSet(TimeZoneSetCallback cb) { timeZoneSetCallback_ = cb; }
  void onAutoResume(AutoResumeCallback cb) { autoResumeCallback_ = cb; }

 private:
  BRMesh &mesh_;
  SceneConfig &scene_;
  WebServer server_;
  uint16_t port_;
  bool started_ = false;
  bool manual_ = false;
  String hostname_;
  LiveDataCallback liveCallback_;
  DateGetCallback dateGetCallback_;
  DateSetCallback dateSetCallback_;
  TimeZoneGetCallback timeZoneGetCallback_;
  TimeZoneSetCallback timeZoneSetCallback_;
  AutoResumeCallback autoResumeCallback_;

  int argInt(const char *name, int defaultValue, int minValue, int maxValue);
  void handleRoot();
  void sendState(int lamp);
  void handleState();
  void handleSet();
  void handleAuto();
  void handleSceneGet();
  void handleScenePost();
  void handleSceneReset();
  void handleLive();
  void handleDateGet();
  void handleDatePost();
  void handleTimeZoneGet();
  void handleTimeZonePost();
};
