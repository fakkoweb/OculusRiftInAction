/************************************************************************************

 Authors     :   Bradley Austin Davis <bdavis@saintandreas.org>
 Copyright   :   Copyright Brad Davis. All Rights reserved.

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.

 ************************************************************************************/

#pragma once

#include <QtOpenGL/QGLWidget>

class QRiftWidget : public QGLWidget, public RiftRenderingApp {
  void paintGL() {
    if (isRenderingConfigured()) {
      draw();
      update();
    }
  }

  virtual void * getRenderWindow() {
    return (void*)effectiveWinId();
  }
public:
  static QGLFormat & getFormat() {
    static QGLFormat glFormat;
    glFormat.setVersion(3, 3);
    glFormat.setProfile(QGLFormat::CoreProfile);
    glFormat.setSampleBuffers(true);
    return glFormat;
  }

  explicit QRiftWidget(QWidget* parent = 0, const QGLWidget* shareWidget = 0, Qt::WindowFlags f = 0) 
    : QGLWidget(parent, shareWidget, f) { 
    initWidget();
  }

  explicit QRiftWidget(QGLContext *context, QWidget* parent = 0, const QGLWidget* shareWidget = 0, Qt::WindowFlags f = 0)
    : QGLWidget(context, parent, shareWidget, f) {
    initWidget();
  }

  explicit QRiftWidget(const QGLFormat& format, QWidget* parent = 0, const QGLWidget* shareWidget = 0, Qt::WindowFlags f = 0)
    : QGLWidget(format, parent, shareWidget, f) {
    initWidget();
  }

  void setupRiftRendering() {
    makeCurrent();
    initGl();
    update();
  }

private:
  void initWidget() {
    setAutoBufferSwap(false);
    bool directHmdMode = false;

    // The ovrHmdCap_ExtendDesktop only reliably reports on Windows currently
    ON_WINDOWS([&] {
      directHmdMode = (0 == (ovrHmdCap_ExtendDesktop & hmd->HmdCaps));
    });

#ifdef BRAD_DEBUG
    widget.setWindowFlags(Qt::FramelessWindowHint);
#endif
    if (!directHmdMode) {
      setWindowFlags(Qt::FramelessWindowHint);
    }
    show();
    if (!directHmdMode) {
      move(hmd->WindowsPos.x, hmd->WindowsPos.y);
    } else {
#ifdef BRAD_DEBUG
      move(0, -1080);
#else
      move(0, 0);
#endif
    }
    resize(hmd->Resolution.w, hmd->Resolution.h);

    // If we're in direct mode, attach to the window
    if (directHmdMode) {
      void * nativeWindowHandle = (void*)(size_t)effectiveWinId();
      if (nullptr != nativeWindowHandle) {
        ovrHmd_AttachToWindow(hmd, nativeWindowHandle, nullptr, nullptr);
      }
    }
  }

};


template <typename T>
class QRiftApplication : public QApplication {
  static int argc;
  static char ** argv;
  std::shared_ptr<T> widget;

public:
  QRiftApplication(int argc, char ** argv) : QApplication(argc, argv) {
    ovr_Initialize();
    widget = std::shared_ptr<T>(new T());
    widget->setupRiftRendering();
  }

  virtual ~QRiftApplication() {
    widget.reset();
    try {
      Platform::runShutdownHooks();
    } catch (...) {

    }
    try {
      ovr_Shutdown();
    } catch (...) {

    }
  }
};

#define RUN_QT_RIFT_WIDGET(WidgetClass) \
  RUN_QT_APP(QRiftApplication<WidgetClass>)