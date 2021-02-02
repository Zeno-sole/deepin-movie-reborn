/*
 * (c) 2017, Deepin Technology Co., Ltd. <support@deepin.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * is provided AS IS, WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, and
 * NON-INFRINGEMENT.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * In addition, as a special exception, the copyright holders give
 * permission to link the code of portions of this program with the
 * OpenSSL library under certain conditions as described in each
 * individual source file, and distribute linked combinations
 * including the two.
 * You must obey the GNU General Public License in all respects
 * for all of the code used other than OpenSSL.  If you modify
 * file(s) with this exception, you may extend this exception to your
 * version of the file(s), but you are not obligated to do so.  If you
 * do not wish to do so, delete this exception statement from your
 * version.  If you delete this exception statement from all source
 * files in the program, then also delete it here.
 */
#include "config.h"

#include "mpv_proxy.h"
#include "mpv_glwidget.h"
#include "compositing_manager.h"
#include "player_engine.h"
#ifndef _LIBDMR_
#include "dmr_settings.h"
#include "movie_configuration.h"
#endif
#include <mpv/client.h>

#include <random>
#include <QtWidgets>
#include <QtGlobal>

#include <xcb/xproto.h>
#include <xcb/xcb_aux.h>
#include <QX11Info>
#include <QLibrary>

namespace dmr {
using namespace mpv::qt;

enum AsyncReplyTag {
    SEEK,
    CHANNEL,
    SPEED
};


//static inline bool command_async(mpv_handle *ctx, const QVariant &args, uint64_t tag)
//{
//    node_builder node(args);
//    int err = mpv_command_node_async(ctx, tag, node.node());
//    return err == 0;
//}

//static inline int set_property_async(mpv_handle *ctx, const QString &name,
//                                     const QVariant &v, uint64_t tag)
//{
//    node_builder node(v);
//    return mpv_set_property_async(ctx, tag, name.toUtf8().data(), MPV_FORMAT_NODE, node.node());
//}

static void mpv_callback(void *d)
{
    MpvProxy *mpv = static_cast<MpvProxy *>(d);
    QMetaObject::invokeMethod(mpv, "has_mpv_events", Qt::QueuedConnection);
}

MpvProxy::MpvProxy(QWidget *parent)
    : Backend(parent)
{
    m_parentWidget = parent;
    if (!CompositingManager::get().composited()) {
        setWindowFlags(Qt::FramelessWindowHint);
        setAttribute(Qt::WA_NativeWindow);
        qDebug() << "proxy hook winId " << this->winId();
    }
#ifdef _LIBDMR_
    firstInit();
    m_bInited = true;
#endif
    //heyi need
//    if (m_creat) {
//        _handle = myHandle::myFromRawHandle(mpv_init());
//        if (CompositingManager::get().composited()) {
//            _gl_widget = new MpvGLWidget(this, _handle);
//            connect(this, &MpvProxy::stateChanged, [ = ]() {
//                _gl_widget->setPlaying(state() != Backend::PlayState::Stopped);
//                _gl_widget->update();
//            });
//        }

//#if defined(USE_DXCB) || defined(_LIBDMR_)
//        _gl_widget->toggleRoundedClip(false);
//#endif
//        auto *layout = new QHBoxLayout(this);
//        layout->setContentsMargins(0, 0, 0, 0);
//        layout->addWidget(_gl_widget);
//        setLayout(layout);
//    }

#if defined (__mips__) || defined (__aarch64__)
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
#endif
}

MpvProxy::~MpvProxy()
{
    disconnect(this, &MpvProxy::has_mpv_events, this, &MpvProxy::handle_mpv_events);
    _connectStateChange = false;
    disconnect(window()->windowHandle(), &QWindow::windowStateChanged, nullptr, nullptr);
    if (CompositingManager::get().composited()) {
        disconnect(this, &MpvProxy::stateChanged, nullptr, nullptr);
        delete _gl_widget;
    }
}

void MpvProxy::initMpvFuns()
{
    qDebug() << "MpvProxy开始initMpvFuns";
    m_waitEvent = reinterpret_cast<mpv_waitEvent>(QLibrary::resolve(libPath("libmpv.so.1"), "mpv_wait_event"));
    m_setOptionString = reinterpret_cast<mpv_set_optionString>(QLibrary::resolve(libPath("libmpv.so.1"), "mpv_set_option_string"));
    m_setProperty = reinterpret_cast<mpv_setProperty>(QLibrary::resolve(libPath("libmpv.so.1"), "mpv_set_property"));
    m_setPropertyAsync = reinterpret_cast<mpv_setProperty_async>(QLibrary::resolve(libPath("libmpv.so.1"), "mpv_set_property_async"));
    m_commandNode = reinterpret_cast<mpv_commandNode>(QLibrary::resolve(libPath("libmpv.so.1"), "mpv_command_node"));
    m_commandNodeAsync = reinterpret_cast<mpv_commandNode_async>(QLibrary::resolve(libPath("libmpv.so.1"), "mpv_command_node_async"));
    m_getProperty = reinterpret_cast<mpv_getProperty>(QLibrary::resolve(libPath("libmpv.so.1"), "mpv_get_property"));
    m_observeProperty = reinterpret_cast<mpv_observeProperty>(QLibrary::resolve(libPath("libmpv.so.1"), "mpv_observe_property"));
    m_eventName = reinterpret_cast<mpv_eventName>(QLibrary::resolve(libPath("libmpv.so.1"), "mpv_event_name"));
    m_creat = reinterpret_cast<mpvCreate>(QLibrary::resolve(libPath("libmpv.so.1"), "mpv_create"));
    m_requestLogMessage = reinterpret_cast<mpv_requestLog_messages>(QLibrary::resolve(libPath("libmpv.so.1"), "mpv_request_log_messages"));
    m_setWakeupCallback = reinterpret_cast<mpv_setWakeup_callback>(QLibrary::resolve(libPath("libmpv.so.1"), "mpv_set_wakeup_callback"));
    m_initialize = reinterpret_cast<mpvinitialize>(QLibrary::resolve(libPath("libmpv.so.1"), "mpv_initialize"));
    m_freeNodecontents = reinterpret_cast<mpv_freeNode_contents>(QLibrary::resolve(libPath("libmpv.so.1"), "mpv_free_node_contents"));
}

void MpvProxy::firstInit()
{
    initMpvFuns();
    if (m_creat) {
        _handle = myHandle::myFromRawHandle(mpv_init());
        if (CompositingManager::get().composited()) {
            _gl_widget = new MpvGLWidget(this, _handle);
            connect(this, &MpvProxy::stateChanged, this, &MpvProxy::slotStateChanged);


#if defined(USE_DXCB) || defined(_LIBDMR_)
            _gl_widget->toggleRoundedClip(false);
#endif
            auto *layout = new QHBoxLayout(this);
            layout->setContentsMargins(0, 0, 0, 0);
            layout->addWidget(_gl_widget);
            setLayout(layout);
            _gl_widget->show();
            //if (_gl_widget) {
            // _gl_widget->initMpvFuns();
            // }
        }
    }
}

mpv_handle *MpvProxy::mpv_init()
{
    //test by heyi
    mpv_handle *h =  static_cast<mpv_handle *>(m_creat());

    bool composited = CompositingManager::get().composited();

    switch (_debugLevel) {
    case DebugLevel::Info:
        m_requestLogMessage(h, "info");
        break;

    case DebugLevel::Debug:
    case DebugLevel::Verbose:
        my_set_property(h, "terminal", "yes");
        if (_debugLevel == DebugLevel::Verbose) {
            my_set_property(h, "msg-level", "all=status");
            m_requestLogMessage(h, "info");

        } else {
            my_set_property(h, "msg-level", "all=v");
            m_requestLogMessage(h, "v");
        }
        break;
    }

#ifdef _LIBDMR_
    if (composited) {
        auto interop = QString::fromUtf8("vaapi-glx");
        if (!qEnvironmentVariableIsEmpty("QT_XCB_GL_INTERGRATION")) {
            auto gl_int = qgetenv("QT_XCB_GL_INTERGRATION");
            if (gl_int == "xcb_egl") {
                interop = "vaapi-egl";
            } else if (gl_int == "xcb_glx") {
                interop = "vaapi-glx";
            } else {
                interop = "auto";
            }
        }
        my_set_property(h, "gpu-hwdec-interop", interop.toUtf8().constData());
        qDebug() << "set gpu-hwdec-interop = " << interop;
    }
    my_set_property(h, "hwdec", "auto");

#else
    if (Settings::get().isSet(Settings::HWAccel)) {
        if (composited) {
            auto disable = Settings::get().disableInterop();
            auto forced = Settings::get().forcedInterop();

            auto interop = QString::fromUtf8("auto");
            switch (CompositingManager::get().interopKind()) {
            case OpenGLInteropKind::INTEROP_AUTO:
                interop = QString::fromUtf8("auto");
                break;

            case OpenGLInteropKind::INTEROP_VAAPI_EGL:
                interop = QString::fromUtf8("vaapi-egl");
                break;

            case OpenGLInteropKind::INTEROP_VAAPI_GLX:
                interop = QString::fromUtf8("vaapi-glx");
                break;

            case OpenGLInteropKind::INTEROP_VDPAU_GLX:
                interop = QString::fromUtf8("vdpau-glx");
                break;

            default:
                break;

            }

            if (!forced.isEmpty()) {
                QStringList valids {"vaapi-egl", "vaapi-glx", "vdpau-glx", "auto"};
                if (valids.contains(forced)) {
                    interop = forced;
                }
            }

            if (!disable) {
                my_set_property(h, "gpu-hwdec-interop", interop.toUtf8().constData());
                qDebug() << "-------- set gpu-hwdec-interop = " << interop
                         << (forced.isEmpty() ? "[detected]" : "[forced]");
            } else {
                qDebug() << "-------- gpu-hwdec-interop is disabled by user";
            }
        }

        if (CompositingManager::get().isOnlySoftDecode()) {
            my_set_property(h, "hwdec", "off");
        } else {
            my_set_property(h, "hwdec", "auto");
        }

    } else {
        my_set_property(h, "hwdec", "off");
    }
#endif
#ifdef __aarch64__
    /*QString path = QString("%1/%2/%3/conf")
                   .arg(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation))
                   .arg(qApp->organizationName())
                   .arg(qApp->applicationName());
    QFile configFile(path);
    if (configFile.exists()) {
        configFile.open(QIODevice::ReadOnly);
        int index = configFile.readLine().left(1).toInt();
        switch (index) {
        case 0:
            my_set_property(h, "hwdec", "no");
            qDebug() << "modify HWDEC no";
            break;
        case 1:
            my_set_property(h, "hwdec", "auto");
            qDebug() << "modify HWDEC auto";
            break;
        case 2:
            my_set_property(h, "hwdec", "yes");
            qDebug() << "modify HWDEC yes";
            break;
        case 3:
            my_set_property(h, "hwdec", "auto");
            qDebug() << "modify HWDEC auto";
            break;
        case 4:
            my_set_property(h, "hwdec", "vdpau");
            qDebug() << "modify HWDEC vdpau";
            break;
        case 5:
            my_set_property(h, "hwdec", "vaapi");
            qDebug() << "modify HWDEC vaapi";
            break;
        default:
            break;
        }
    }*/
    if (CompositingManager::get().isOnlySoftDecode()) {
        my_set_property(h, "hwdec", "off");
    } else {
        my_set_property(h, "hwdec", "auto");
    }
    qDebug() << "modify HWDEC auto";
#endif
    my_set_property(h, "panscan", 1.0);
    //my_set_property(h, "no-keepaspect", "true");

    if (composited) {
        //vo=gpu seems broken, it'll makes video output into a seperate window
        //my_set_property(h, "vo", "gpu");
#ifdef __mips__
        m_setOptionString(h, "vo", "opengl-cb");
        m_setOptionString(h, "hwdec-preload", "auto");
        m_setOptionString(h, "opengl-hwdec-interop", "auto");
        m_setOptionString(h, "hwdec", "auto");
        qDebug() << "-------- __mips__hwdec____________";
#else
        my_set_property(h, "vo", "libmpv,opengl-cb");
        my_set_property(h, "vd-lavc-dr", "no");
        my_set_property(h, "gpu-sw", "on");
        //my_set_property(h, "ao", "alsa");
#endif
    } else {
#if defined (__mips__) || defined (__aarch64__)
        if (CompositingManager::get().hascard()) {
            if (CompositingManager::get().isOnlySoftDecode()) {
                my_set_property(h, "hwdec", "off");
            } else {
                my_set_property(h, "hwdec", "auto");
            }
#ifdef __mips__
            my_set_property(h, "vo", "vdpau"); //longxin 独显--vo=vpdau
#else
            my_set_property(h, "vo", "gpu");
#endif
        } else {
            my_set_property(h, "vo", "xv,x11");
            my_set_property(h, "ao", "alsa");
        }
#else
#ifdef MWV206_0
        QFileInfo fi("/dev/mwv206_0");              //景嘉微显卡目前只支持vo=xv，等日后升级代码需要酌情修改。
        if (fi.exists()) {
            my_set_property(h, "hwdec", "vdpau");
            my_set_property(h, "vo", "vdpau");
        } else {
            auto e = QProcessEnvironment::systemEnvironment();
            QString XDG_SESSION_TYPE = e.value(QStringLiteral("XDG_SESSION_TYPE"));
            QString WAYLAND_DISPLAY = e.value(QStringLiteral("WAYLAND_DISPLAY"));

            if (XDG_SESSION_TYPE == QLatin1String("wayland") ||
                    WAYLAND_DISPLAY.contains(QLatin1String("wayland"), Qt::CaseInsensitive)) {
                my_set_property(h, "vo", "gpu,x11,xv");
            } else {
                my_set_property(h, "vo", "xv");
            }

        }
#else
        my_set_property(h, "vo", "gpu,xv,x11");
#endif
#endif
        my_set_property(h, "wid", m_parentWidget->winId());
    }
    if (QFile::exists("/dev/csmcore")) {
        my_set_property(h, "vo", "xv,x11");
        my_set_property(h, "hwdec", "auto");
    }
    qDebug() << __func__ << my_get_property(h, "vo").toString();

#ifdef __mips__
    if (!CompositingManager::get().hascard()) {
        qInfo() << "修改音视频同步模式";
        my_set_property(h, "video-sync", "desync");
    }
#endif
    //QLocale locale;
    QString strMovie = QObject::tr("Movie");
    /*if (locale.language() == QLocale::Chinese) { //获取系统语言环境
        qDebug() << "Chinese system" ;
        strMovie = "影院";
    }*/
    //设置音量名称
    my_set_property(h, "audio-client-name", strMovie);
    //my_set_property(h, "keepaspect-window", "no");
    //设置视频固定帧率，暂时无效
    //my_set_property(h, "correct-pts", false);
    //my_set_property(h, "fps", 30);
    my_set_property(h, "panscan", 0);
    my_set_property(h, "volume-max", 100.0);
    my_set_property(h, "input-cursor", "no");
    my_set_property(h, "cursor-autohide", "no");

    //my_set_property(h, "sub-ass-override", "yes");
    //my_set_property(h, "sub-ass-style-override", "yes");
    my_set_property(h, "sub-auto", "fuzzy");
    my_set_property(h, "sub-visibility", "true");
    //my_set_property(h, "sub-scale-with-window", "no");
    //my_set_property(h, "sub-scale-by-window", "no");
    my_set_property(h, "sub-pos", 100);
    my_set_property(h, "sub-margin-y", 36);
    my_set_property(h, "sub-border-size", 0);

    my_set_property(h, "screenshot-template", "deepin-movie-shot%n");
    my_set_property(h, "screenshot-directory", "/tmp");

#ifndef _LIBDMR_
    if (Settings::get().isSet(Settings::ResumeFromLast)) {
        my_set_property(h, "save-position-on-quit", true);
    }
#endif

    //only to get notification without data
    m_observeProperty(h, 0, "time-pos", MPV_FORMAT_NONE); //playback-time ?
    m_observeProperty(h, 0, "pause", MPV_FORMAT_NONE);
    m_observeProperty(h, 0, "mute", MPV_FORMAT_NONE);
    m_observeProperty(h, 0, "volume", MPV_FORMAT_NONE); //ao-volume ?
    m_observeProperty(h, 0, "sid", MPV_FORMAT_NONE);
    m_observeProperty(h, 0, "aid", MPV_FORMAT_NODE);
    m_observeProperty(h, 0, "dwidth", MPV_FORMAT_NODE);
    m_observeProperty(h, 0, "dheight", MPV_FORMAT_NODE);

    // because of vpu, we need to implement playlist w/o mpv
    //m_observeProperty(h, 0, "playlist-pos", MPV_FORMAT_NONE);
    //m_observeProperty(h, 0, "playlist-count", MPV_FORMAT_NONE);
    m_observeProperty(h, 0, "core-idle", MPV_FORMAT_NODE);
    m_observeProperty(h, 0, "paused-for-cache", MPV_FORMAT_NODE);

    m_setWakeupCallback(h, mpv_callback, this);
    connect(this, &MpvProxy::has_mpv_events, this, &MpvProxy::handle_mpv_events,
            Qt::DirectConnection);
    if (m_initialize(h) < 0) {
        std::runtime_error("mpv init failed");
    }

    //load profile
    auto ol = CompositingManager::get().getBestProfile();
    auto p = ol.begin();
    while (p != ol.end()) {
        if (!p->first.startsWith("#")) {
#if !defined (__mips__ ) && !defined(__aarch64__)
#ifdef MWV206_0
            QFileInfo fi("/dev/mwv206_0");              //景嘉微显卡目前只支持vo=xv，等日后升级代码需要酌情修改。
            if (!fi.exists()) {
                my_set_property(h, p->first.toUtf8().constData(), p->second.toUtf8().constData());
                qDebug() << "apply" << p->first << "=" << p->second;
            }
#else
            my_set_property(h, p->first.toUtf8().constData(), p->second.toUtf8().constData());
            qDebug() << "apply" << p->first << "=" << p->second;
#endif
#endif
        } else {
            qDebug() << "ignore(commented out)" << p->first << "=" << p->second;
        }
        ++p;
    }

    return h;
}

void MpvProxy::setState(PlayState s)
{
    if (_state != s) {
        _state = s;
        if (_gl_widget) {
            _gl_widget->setPlaying(s != PlayState::Stopped);
        }
        emit stateChanged();
    }
}


void MpvProxy::pollingEndOfPlayback()
{
    if (_state != Backend::Stopped) {
        _polling = true;
        blockSignals(true);
        stop();
        auto idle = my_get_property(_handle, "idle-active").toBool();
        if (idle) {
            blockSignals(false);
            setState(Backend::Stopped);
            _polling = false;
            return;
        }

        while (_state != Backend::Stopped) {
            mpv_event *ev = m_waitEvent(_handle, 0.005);
            if (ev->event_id == MPV_EVENT_NONE)
                continue;

            if (ev->event_id == MPV_EVENT_END_FILE) {
                qDebug() << "end of playback";
                blockSignals(false);
                setState(Backend::Stopped);
                break;
            }
        }
        _polling = false;
    }
}

void MpvProxy::pollingStartOfPlayback()
{
    if (_state == Backend::PlayState::Stopped) {
        _polling = true;

        while (_state == Backend::Stopped) {
            mpv_event *ev = m_waitEvent(_handle, 0.005);
            if (ev->event_id == MPV_EVENT_NONE)
                continue;

            if (ev->event_id == MPV_EVENT_FILE_LOADED) {
                qDebug() << "start of playback";
                setState(Backend::Playing);
                break;
            }
        }

        _polling = false;
    }
}

const PlayingMovieInfo &MpvProxy::playingMovieInfo()
{
    return _pmf;
}

void MpvProxy::handle_mpv_events()
{
    while (1) {
        mpv_event *ev = m_waitEvent(_handle, 0.0005);
        if (ev->event_id == MPV_EVENT_NONE)
            break;

        switch (ev->event_id) {
        case MPV_EVENT_LOG_MESSAGE:
            processLogMessage(reinterpret_cast<mpv_event_log_message *>(ev->data));
            break;

        case MPV_EVENT_PROPERTY_CHANGE:
            processPropertyChange(reinterpret_cast<mpv_event_property *>(ev->data));
            break;

        case MPV_EVENT_COMMAND_REPLY:
            if (ev->error < 0) {
                qDebug() << "command error";
            }

            if (ev->reply_userdata == AsyncReplyTag::SEEK) {
                this->_pendingSeek = false;
            }
            break;

        case MPV_EVENT_PLAYBACK_RESTART:
            // caused by seek or just playing
            break;

        case MPV_EVENT_TRACKS_CHANGED:
            qDebug() << m_eventName(ev->event_id);
            updatePlayingMovieInfo();
            emit tracksChanged();
            break;

        case MPV_EVENT_FILE_LOADED: {
            qDebug() << m_eventName(ev->event_id);

            if (_gl_widget) {
                qDebug() << "hwdec-interop" << my_get_property(_handle, "gpu-hwdec-interop")
                         << "codec: " << my_get_property(_handle, "video-codec")
                         << "format: " << my_get_property(_handle, "video-format");
            }
#ifdef __mips__
            qDebug() << "MPV_EVENT_FILE_LOADED __mips__";
            auto codec = my_get_property(_handle, "video-codec").toString();
            auto name = my_get_property(_handle, "filename").toString();
            if (codec.toLower().contains("wmv3") || codec.toLower().contains("wmv2") || codec.toLower().contains("mpeg2video") ||
                    name.toLower().contains("wmv")) {
                qDebug() << "my_set_property hwdec no";
                my_set_property(_handle, "hwdec", "no");
            }
#endif
#ifdef __aarch64__
            qDebug() << "MPV_EVENT_FILE_LOADED aarch64";
            auto codec = my_get_property(_handle, "video-codec").toString();
            if (codec.toLower().contains("wmv3") || codec.toLower().contains("wmv2") || codec.toLower().contains("mpeg2video")) {
//                    qDebug() << "my_set_property hwdec no";
//                    my_set_property(_handle, "hwdec", "no");
                qDebug() << "my_set_property hwdec auto";
                if (CompositingManager::get().isOnlySoftDecode()) {
                    my_set_property(_handle, "hwdec", "off");
                } else {
                    my_set_property(_handle, "hwdec", "auto");
                }
            }
#endif
            setState(PlayState::Playing); //might paused immediately
            emit fileLoaded();
            qDebug() << QString("rotate metadata: dec %1, out %2")
                     .arg(my_get_property(_handle, "video-dec-params/rotate").toInt())
                     .arg(my_get_property(_handle, "video-params/rotate").toInt());
            break;
        }
        case MPV_EVENT_VIDEO_RECONFIG: {
            auto sz = videoSize();
            if (!sz.isEmpty())
                emit videoSizeChanged();
            qDebug() << "videoSize " << sz;
            break;
        }

        case MPV_EVENT_END_FILE: {
#ifndef _LIBDMR_
            MovieConfiguration::get().updateUrl(this->_file,
                                                ConfigKnownKey::StartPos, 0);
#endif
            mpv_event_end_file *ev_ef = reinterpret_cast<mpv_event_end_file *>(ev->data);
            qDebug() << m_eventName(ev->event_id) <<
                     "reason " << ev_ef->reason;

            setState(PlayState::Stopped);
            break;
        }

        case MPV_EVENT_IDLE:
            qDebug() << m_eventName(ev->event_id);
            setState(PlayState::Stopped);
            emit elapsedChanged();
            break;

        default:
            qDebug() << m_eventName(ev->event_id);
            break;
        }
    }
}

void MpvProxy::processLogMessage(mpv_event_log_message *ev)
{
    switch (ev->log_level) {
    case MPV_LOG_LEVEL_WARN:
        qWarning() << QString("%1: %2").arg(ev->prefix).arg(ev->text);
        emit mpvWarningLogsChanged(QString(ev->prefix), QString(ev->text));
        break;

    case MPV_LOG_LEVEL_ERROR:
    case MPV_LOG_LEVEL_FATAL: {
        QString strError = ev->text;
        if (strError.contains("Failed setup for format vdpau")) {
            m_bLastIsSpecficFormat = true;
        }
        qCritical() << QString("%1: %2").arg(ev->prefix).arg(strError);
        emit mpvErrorLogsChanged(QString(ev->prefix), strError);
    }
    break;

    case MPV_LOG_LEVEL_INFO:
        qInfo() << QString("%1: %2").arg(ev->prefix).arg(ev->text);
        break;

    default:
        qDebug() << QString("%1: %2").arg(ev->prefix).arg(ev->text);
        break;
    }
}

void MpvProxy::processPropertyChange(mpv_event_property *ev)
{
    //if (ev->data == NULL) return;

    QString name = QString::fromUtf8(ev->name);
    if (name != "time-pos") qDebug() << name;

    if (name == "time-pos") {
        emit elapsedChanged();
    } else if (name == "volume") {
        emit volumeChanged();
    } else if (name == "dwidth" || name == "dheight") {
        auto sz = videoSize();
        if (!sz.isEmpty())
            emit videoSizeChanged();
        qDebug() << "update videoSize " << sz;
    } else if (name == "aid") {
        emit aidChanged();
    } else if (name == "sid") {
        if (_externalSubJustLoaded) {
#ifndef _LIBDMR_
            MovieConfiguration::get().updateUrl(this->_file, ConfigKnownKey::SubId, sid());
#endif
            _externalSubJustLoaded = false;
        }
        emit sidChanged();
    } else if (name == "mute") {
        emit muteChanged();
    } else if (name == "sub-visibility") {
        //_hideSub = my_get_property(_handle, "sub-visibility")
    } else if (name == "pause") {
        auto idle = my_get_property(_handle, "idle-active").toBool();
        if (my_get_property(_handle, "pause").toBool()) {
            if (!idle)
                setState(PlayState::Paused);
            else
                my_set_property(_handle, "pause", false);
        } else {
            if (state() != PlayState::Stopped) {
                setState(PlayState::Playing);
                if (_startPlayDuration != 0) {
                    seekAbsolute(static_cast<int>(_startPlayDuration));
                    _startPlayDuration = 0;
                }
            }
        }
    } else if (name == "core-idle") {
    } else if (name == "paused-for-cache") {
        qDebug() << "paused-for-cache" << my_get_property_variant(_handle, "paused-for-cache");
        emit urlpause(my_get_property_variant(_handle, "paused-for-cache").toBool());
    }
}

bool MpvProxy::loadSubtitle(const QFileInfo &fi)
{
    //movie could be in an inner state that marked as Stopped when loadfile executes
    //if (state() == PlayState::Stopped) { return true; }
    if (!m_bInited) {
        firstInit();
        m_bInited = true;
    }

    if (!fi.exists())
        return false;

    QList<QVariant> args = { "sub-add", fi.absoluteFilePath(), "select" };
    qDebug() << args;
    QVariant id = my_command(_handle, args);
    if (id.canConvert<ErrorReturn>()) {
        return false;
    }

    // by settings this flag, we can match the corresponding sid change and save it
    // in the movie database
    _externalSubJustLoaded = true;
    return true;
}

bool MpvProxy::isSubVisible()
{
    return my_get_property(_handle, "sub-visibility").toBool();
}

void MpvProxy::setSubDelay(double secs)
{
    my_set_property(_handle, "sub-delay", secs);
#ifndef _LIBDMR_
    MovieConfiguration::get().updateUrl(_file, ConfigKnownKey::SubDelay, subDelay());
#endif
}

double MpvProxy::subDelay() const
{
    return my_get_property(_handle, "sub-delay").toDouble();
}

QString MpvProxy::subCodepage()
{
    auto cp = my_get_property(_handle, "sub-codepage").toString();
    if (cp.startsWith("+")) {
        cp.remove(0, 1);
    }

    return cp;
}

void MpvProxy::addSubSearchPath(const QString &path)
{
    my_set_property(_handle, "sub-paths", path);
    my_set_property(_handle, "sub-file-paths", path);
}

void MpvProxy::setSubCodepage(const QString &cp)
{
    if (!m_bInited) {
        m_bInited = true;
        firstInit();
    }

    auto cp2 = cp;
    if (!cp.startsWith("+") && cp != "auto")
        cp2.prepend('+');

    my_set_property(_handle, "sub-codepage", cp2);
    my_command(_handle, {"sub-reload"});
#ifndef _LIBDMR_
    if (_file.isValid())
        MovieConfiguration::get().updateUrl(_file, ConfigKnownKey::SubCodepage, subCodepage());
#endif
}

void MpvProxy::updateSubStyle(const QString &font, int sz)
{
    my_set_property(_handle, "sub-font", font);
    my_set_property(_handle, "sub-font-size", sz);
    my_set_property(_handle, "sub-color", "#FFFFFF");
    my_set_property(_handle, "sub-border-size", 1);
    my_set_property(_handle, "sub-border-color", "0.0/0.0/0.0/0.50");
    my_set_property(_handle, "sub-shadow-offset", 1);
    my_set_property(_handle, "sub-shadow-color", "0.0/0.0/0.0/0.50");
}

void MpvProxy::showEvent(QShowEvent *re)
{
    if (!_connectStateChange) {
//        connect(window()->windowHandle(), &QWindow::windowStateChanged, [ = ](Qt::WindowState ws) {
//            //设置视频按比例黑边填充
//            my_set_property(_handle, "panscan",0);
//                         //(ws != Qt::WindowMaximized && ws != Qt::WindowFullScreen) ? 1.0 : 0.0);

//        });
        _connectStateChange = true;
    }
    Backend::showEvent(re);
}

void MpvProxy::resizeEvent(QResizeEvent *re)
{
    if (state() == PlayState::Stopped) {
        return;
    }
    Backend::resizeEvent(re);
}

void MpvProxy::savePlaybackPosition()
{
    if (state() == PlayState::Stopped) {
        return;
    }

#ifndef _LIBDMR_
    MovieConfiguration::get().updateUrl(this->_file, ConfigKnownKey::SubId, sid());
    if (elapsed() - 10 >= 0) {
        MovieConfiguration::get().updateUrl(this->_file, ConfigKnownKey::StartPos, elapsed() - 10);
    } else {
        MovieConfiguration::get().updateUrl(this->_file, ConfigKnownKey::StartPos, 10);
    }
#endif
}

void MpvProxy::setPlaySpeed(double times)
{
    //my_set_property(_handle, "speed", times);
    my_set_property_async(_handle, "speed", times, AsyncReplyTag::SPEED);
}

void MpvProxy::selectSubtitle(int id)
{
    if (id > _pmf.subs.size()) {
        id = _pmf.subs.size() == 0 ? -1 : _pmf.subs[0]["id"].toInt();
    }

    my_set_property(_handle, "sid", id);
#ifndef _LIBDMR_
    MovieConfiguration::get().updateUrl(_file, ConfigKnownKey::SubId, sid());
#endif
}

void MpvProxy::toggleSubtitle()
{
    if (state() == PlayState::Stopped) {
        return;
    }

    my_set_property(_handle, "sub-visibility", !isSubVisible());
}

int MpvProxy::aid() const
{
    return my_get_property(_handle, "aid").toInt();
}

int MpvProxy::sid() const
{
    return my_get_property(_handle, "sid").toInt();
}

void MpvProxy::selectTrack(int id)
{
    if (id >= _pmf.audios.size()) return;
    auto sid = _pmf.audios[id]["id"];
    my_set_property(_handle, "aid", sid);
}

void MpvProxy::changeSoundMode(SoundMode sm)
{
    QList<QVariant> args;

    switch (sm) {
    case SoundMode::Stereo:
        args << "af" << "set" << "stereotools=muter=false";
        break;
    case SoundMode::Left:
        args << "af" << "set" << "stereotools=muter=true";
        break;
    case SoundMode::Right:
        args << "af" << "set" << "stereotools=mutel=true";
        break;
    }

    my_command(_handle, args);
}

void MpvProxy::volumeUp()
{
    if (!m_bInited) {
        firstInit();
        m_bInited = true;
    }

    if (volume() >= 200)
        return;
//    QList<QVariant> args = { "add", "volume", 8 };
//    qDebug () << args;
//    my_command(_handle, args);
    changeVolume(volume() + 10);
}

void MpvProxy::changeVolume(int val)
{
    if (!m_bInited) {
        firstInit();
        m_bInited = true;
    }

    //val += 40;
    //val = qMin(qMax(val, 40), 240);
    my_set_property(_handle, "volume", volumeCorrection(val));
}

void MpvProxy::volumeDown()
{
    if (volume() <= 0)
        return;
//    QList<QVariant> args = { "add", "volume", -8 };
//    qDebug () << args;
//    my_command(_handle, args);
    changeVolume(volume() - 10);
}

int MpvProxy::volume() const
{
    /*float actualVol = my_get_property(_handle, "volume").toFloat();
    if (actualVol > 0 && actualVol < 50) {
        return (actualVol - 40) * 5;
    } else if (actualVol > 80 && actualVol < 200) {
        return (actualVol - 80) * 10;
    } else
    <<<<<<< Updated upstream
        return get_property(_handle, "volume").toInt();*/
    int actualVol = my_get_property(_handle, "volume").toInt();
    int dispaly = static_cast<int>((actualVol - 40) / 60.0 * 200.0);
    return dispaly;
}

int MpvProxy::videoRotation() const
{
    auto vr = my_get_property(_handle, "video-rotate").toInt();
    return (vr + 360) % 360;
}

void MpvProxy::setVideoRotation(int degree)
{
    my_set_property(_handle, "video-rotate", degree);
}

void MpvProxy::setVideoAspect(double r)
{
    my_set_property(_handle, "video-aspect", r);
}

double MpvProxy::videoAspect() const
{
    return my_get_property(_handle, "video-aspect").toDouble();
}

bool MpvProxy::muted() const
{
    return my_get_property(_handle, "mute").toBool();
}

void MpvProxy::toggleMute()
{
    QList<QVariant> args = { "cycle", "mute" };
    qDebug() << args;
    if (!m_bInited) {
        firstInit();
        m_bInited = true;
    }
    my_command(_handle, args);
}

void MpvProxy::slotStateChanged()
{
    _gl_widget->setPlaying(state() != Backend::PlayState::Stopped);
    _gl_widget->update();
}

void MpvProxy::play()
{
    if (!m_bInited) {
        firstInit();
        m_bInited = true;
    }

    QList<QVariant> args = { "loadfile" };
    QStringList opts = { };

    if (_file.isLocalFile()) {
        args << QFileInfo(_file.toLocalFile()).absoluteFilePath();
    } else {
        args << _file.url();
    }
#ifndef _LIBDMR_
    auto cfg = MovieConfiguration::get().queryByUrl(_file);
    auto key = MovieConfiguration::knownKey2String(ConfigKnownKey::StartPos);
    if (Settings::get().isSet(Settings::ResumeFromLast) && cfg.contains(key)) {
        //opts << QString("start=%1").arg(0);   //如果视频长度小于1s这段代码会导致视频无法播放
        _startPlayDuration = cfg[key].toInt();
    }

    key = MovieConfiguration::knownKey2String(ConfigKnownKey::SubCodepage);
    if (cfg.contains(key)) {
        opts << QString("sub-codepage=%1").arg(cfg[key].toString());
    }

    key = MovieConfiguration::knownKey2String(ConfigKnownKey::SubDelay);
    if (cfg.contains(key)) {
        opts << QString("sub-delay=%1").arg(cfg[key].toDouble());
    }

    if (!_dvdDevice.isEmpty()) {
        opts << QString("dvd-device=%1").arg(_dvdDevice);
    }

    // hwdec could be disabled by some codecs, so we need to re-enable it
    if (Settings::get().isSet(Settings::HWAccel) && m_bLastIsSpecficFormat) {
        my_set_property(_handle, "hwdec", "auto");
#if defined (__mips__) || defined (__aarch64__)
        if (!CompositingManager::get().hascard() || CompositingManager::get().isOnlySoftDecode()) {
            my_set_property(_handle, "hwdec", "off");
        }
#endif
    }
#else
    if (CompositingManager::get().isOnlySoftDecode()) {
        my_set_property(_handle, "hwdec", "off");
    } else {
        my_set_property(_handle, "hwdec", "auto");
    }
#endif
#ifdef __mips__
    qDebug() << "play __mips__";
    auto codec = my_get_property(_handle, "video-codec").toString();
    if (codec.toLower().contains("wmv3") || codec.toLower().contains("wmv2") || codec.toLower().contains("mpeg2video")) {
        qDebug() << "my_set_property hwdec no";
        my_set_property(_handle, "hwdec", "no");
        m_bLastIsSpecficFormat = true;
    } else {
        m_bLastIsSpecficFormat = false;
    }
#endif
#ifdef __aarch64__
    qDebug() << "MPV_EVENT_FILE_LOADED aarch64";
    auto codec = my_get_property(_handle, "video-codec").toString();
    if (codec.toLower().contains("wmv3") || codec.toLower().contains("wmv2") || codec.toLower().contains("mpeg2video")) {
        qDebug() << "my_set_property hwdec no";
        my_set_property(_handle, "hwdec", "no");
        m_bLastIsSpecficFormat = true;
    } else {
        m_bLastIsSpecficFormat = false;
    }
#endif

    QFileInfo fi("/dev/mwv206_0");
    if (fi.exists()) {
        my_set_property(_handle, "hwdec", "vdpau");
        my_set_property(_handle, "vo", "vdpau");
    }

    if (opts.size()) {
        //opts << "sub-auto=fuzzy";
        args << "replace" << opts.join(',');
    }

    qDebug() << args;
    my_command(_handle, args);
    my_set_property(_handle, "pause", _pauseOnStart);

#ifndef _LIBDMR_
    // by giving a period of time, movie will be loaded and auto-loaded subs are
    // all ready, then load extra subs from db
    // this keeps order of subs
    QTimer::singleShot(100, [this]() {
        auto cfg = MovieConfiguration::get().queryByUrl(_file);
        auto ext_subs = MovieConfiguration::get().getListByUrl(_file, ConfigKnownKey::ExternalSubs);
        for (const auto &sub : ext_subs) {
            if (!QFile::exists(sub)) {
                MovieConfiguration::get().removeFromListUrl(_file, ConfigKnownKey::ExternalSubs, sub);
            } else {
                loadSubtitle(sub);
            }
        }

        auto key = MovieConfiguration::knownKey2String(ConfigKnownKey::SubId);
        if (cfg.contains(key)) {
            selectSubtitle(cfg[key].toInt());
        }
    });
#endif
}


void MpvProxy::pauseResume()
{
    if (_state == PlayState::Stopped)
        return;

    my_set_property(_handle, "pause", !paused());
}

void MpvProxy::stop()
{
    QList<QVariant> args = { "stop" };
    qDebug() << args;
    my_command(_handle, args);
}

QImage MpvProxy::takeScreenshot()
{
    return takeOneScreenshot();
}

void MpvProxy::burstScreenshot()
{
    if (_inBurstShotting) {
        qWarning() << "already in burst screenshotting mode";
        return;
    }

    if (state() == PlayState::Stopped)
        return;

    //my_command(_handle, QList<QVariant> {"revert-seek", "mark"});
    _posBeforeBurst = my_get_property(_handle, "time-pos");

    int d = static_cast<int>(duration() / 15);

    std::random_device rd;
    std::mt19937 g(rd());
    std::uniform_int_distribution<int> uniform_dist(0, d);
    _burstPoints.clear();
    for (int i = 0; i < 15; i++) {
        _burstPoints.append(d * i + uniform_dist(g));
    }
    _burstStart = 0;

    if (duration() < 35) {
        emit notifyScreenshot(QImage(), 0);
        stopBurstScreenshot();
        return;
    }
    qDebug() << "burst span " << _burstPoints;

    if (!paused()) pauseResume();
    _inBurstShotting = true;
    QTimer::singleShot(0, this, &MpvProxy::stepBurstScreenshot);
}

qint64 MpvProxy::nextBurstShootPoint()
{
    auto next = _burstPoints[static_cast<int>(_burstStart++)];
    if (next >= duration()) {
        next = duration() - 5;
    }

    return next;
}

int MpvProxy::volumeCorrection(int displayVol)
{
    int realVol = static_cast<int>((displayVol / 200.0) * 60.0 + 40);
    return (realVol);
}

QVariant MpvProxy::my_get_property(mpv_handle *ctx, const QString &name) const
{
    mpv_node node;
    if (!m_getProperty) return QVariant();
    int err = m_getProperty(ctx, name.toUtf8().data(), MPV_FORMAT_NODE, &node);
    if (err < 0)
        return QVariant::fromValue(ErrorReturn(err));
    my_node_autofree f(&node);
    return node_to_variant(&node);
}

int MpvProxy::my_set_property(mpv_handle *ctx, const QString &name, const QVariant &v)
{
    node_builder node(v);
    if (!m_setProperty) return 0;
    return m_setProperty(ctx, name.toUtf8().data(), MPV_FORMAT_NODE, node.node());
}

bool MpvProxy::my_command_async(mpv_handle *ctx, const QVariant &args, uint64_t tag)
{
    node_builder node(args);
    int err = m_commandNodeAsync(ctx, tag, node.node());
    return err == 0;
}

int MpvProxy::my_set_property_async(mpv_handle *ctx, const QString &name, const QVariant &v, uint64_t tag)
{
    node_builder node(v);
    return m_setPropertyAsync(ctx, tag, name.toUtf8().data(), MPV_FORMAT_NODE, node.node());
}

QVariant MpvProxy::my_get_property_variant(mpv_handle *ctx, const QString &name)
{
    mpv_node node;
    if (m_getProperty(ctx, name.toUtf8().data(), MPV_FORMAT_NODE, &node) < 0)
        return QVariant();
    my_node_autofree f(&node);
    return node_to_variant(&node);
}

QVariant MpvProxy::my_command(mpv_handle *ctx, const QVariant &args)
{
    node_builder node(args);
    mpv_node res;
    int err = m_commandNode(ctx, node.node(), &res);
    if (err < 0)
        return QVariant::fromValue(ErrorReturn(err));
    my_node_autofree f(&res);
    return node_to_variant(&res);
}

QImage MpvProxy::takeOneScreenshot()
{
    if (state() == PlayState::Stopped) return QImage();

    QList<QVariant> args = {"screenshot-raw"};
    node_builder node(args);
    mpv_node res;
    int err = m_commandNode(_handle, node.node(), &res);
    if (err < 0) {
        qWarning() << "screenshot raw failed";
        return QImage();
    }

    my_node_autofree f(&res);

    Q_ASSERT(res.format == MPV_FORMAT_NODE_MAP);

    int w = 0, h = 0, stride = 0;

    mpv_node_list *list = res.u.list;
    uchar *data = nullptr;

    for (int n = 0; n < list->num; n++) {
        auto key = QString::fromUtf8(list->keys[n]);
        if (key == "w") {
            w = static_cast<int>(list->values[n].u.int64);
        } else if (key == "h") {
            h = static_cast<int>(list->values[n].u.int64);
        } else if (key == "stride") {
            stride = static_cast<int>(list->values[n].u.int64);
        } else if (key == "format") {
            auto format = QString::fromUtf8(list->values[n].u.string);
            qDebug() << "format" << format;
        } else if (key == "data") {
            data = static_cast<uchar *>(list->values[n].u.ba->data);
        }
    }

    if (data) {
        //alpha should be ignored
        auto img = QImage(static_cast<const uchar *>(data), w, h, stride, QImage::Format_RGB32);
        img.bits();
        int rotationdegree = videoRotation();
#ifdef __mips__
        if (rotationdegree && CompositingManager::get().hascard()) {      //龙芯的vo改为vdpau,需要自己旋转
            QMatrix matrix;
            matrix.rotate(rotationdegree);
            img = QPixmap::fromImage(img).transformed(matrix, Qt::SmoothTransformation).toImage();
        }
#else
        if (rotationdegree && CompositingManager::get().composited()) {      //只有opengl窗口需要自己旋转
            QMatrix matrix;
            matrix.rotate(rotationdegree);
            img = QPixmap::fromImage(img).transformed(matrix, Qt::SmoothTransformation).toImage();
        }
#endif
        return img;
    }

    qDebug() << "failed";
    return QImage();
}

void MpvProxy::stepBurstScreenshot()
{
    if (!_inBurstShotting) {
        return;
    }

    auto pos = nextBurstShootPoint();
    my_command(_handle, QList<QVariant> {"seek", pos, "absolute"});
//    int tries = 10;
    while (true) {
        mpv_event *ev = m_waitEvent(_handle, 0.005);
        if (ev->event_id == MPV_EVENT_NONE)
            continue;

        if (ev->event_id == MPV_EVENT_PLAYBACK_RESTART) {
            qDebug() << "seek finished" << elapsed();
            break;
        }

        if (ev->event_id == MPV_EVENT_END_FILE) {
            qDebug() << "seek finished (end of file)" << elapsed();
            break;
        }
    }

    QImage img = takeOneScreenshot();
    if (img.isNull()) {
        emit notifyScreenshot(img, elapsed());
        stopBurstScreenshot();
        return;
    }
    emit notifyScreenshot(img, elapsed());

    QTimer::singleShot(0, this, &MpvProxy::stepBurstScreenshot);
}

void MpvProxy::stopBurstScreenshot()
{
    _inBurstShotting = false;
    //my_command(_handle, QList<QVariant> {"revert-seek", "mark"});
    my_set_property(_handle, "time-pos", _posBeforeBurst);
}

void MpvProxy::seekForward(int secs)
{
    if (state() == PlayState::Stopped) return;

    if (_pendingSeek) return;
    QList<QVariant> args = { "seek", QVariant(secs), "relative+exact" };
    qDebug() << args;
    my_command_async(_handle, args, AsyncReplyTag::SEEK);
    _pendingSeek = true;
}

void MpvProxy::seekBackward(int secs)
{
    if (state() == PlayState::Stopped) return;

    if (_pendingSeek) return;
    if (secs > 0) secs = -secs;
    QList<QVariant> args = { "seek", QVariant(secs), "relative+exact" };
    qDebug() << args;
    my_command_async(_handle, args, AsyncReplyTag::SEEK);
    _pendingSeek = true;
}

void MpvProxy::seekAbsolute(int pos)
{
    if (state() == PlayState::Stopped) return;

    if (_pendingSeek) return;
    QList<QVariant> args = { "seek", QVariant(pos), "absolute" };
    qDebug() << args;
    //command(_handle, args);
    _pendingSeek = true;
    my_command_async(_handle, args, AsyncReplyTag::SEEK);
}

QSize MpvProxy::videoSize() const
{
    if (state() == PlayState::Stopped) return QSize(-1, -1);
    auto sz = QSize(my_get_property(_handle, "dwidth").toInt(),
                    my_get_property(_handle, "dheight").toInt());

    auto r = my_get_property(_handle, "video-out-params/rotate").toInt();
    if (r == 90 || r == 270) {
        sz.transpose();
    }

    return sz;
}

qint64 MpvProxy::duration() const
{
    return my_get_property(_handle, "duration").value<qint64>();
}


qint64 MpvProxy::elapsed() const
{
    if (state() == PlayState::Stopped) return 0;
    return  my_get_property(_handle, "time-pos").value<qint64>();

}

//void MpvProxy::changeProperty(const QString &name, const QVariant &v)
//{
//}

void MpvProxy::updatePlayingMovieInfo()
{
    _pmf.subs.clear();
    _pmf.audios.clear();

    auto v = my_get_property(_handle, "track-list").toList();
    auto p = v.begin();
    while (p != v.end()) {
        const auto &t = p->toMap();
        if (t["type"] == "audio") {
            AudioInfo ai;
            ai["type"] = t["type"];
            ai["id"] = t["id"];
            ai["lang"] = t["lang"];
            ai["external"] = t["external"];
            ai["external-filename"] = t["external-filename"];
            ai["selected"] = t["selected"];
            ai["title"] = t["title"];

            if (t["title"].toString().size() == 0) {
                if (t["lang"].isValid() && t["lang"].toString().size() && t["lang"].toString() != "und")
                    ai["title"] = t["lang"];
                else if (!t["external"].toBool())
                    ai["title"] = "[internal]";
            }


            _pmf.audios.append(ai);
        } else if (t["type"] == "sub") {
            SubtitleInfo si;
            si["type"] = t["type"];
            si["id"] = t["id"];
            si["lang"] = t["lang"];
            si["external"] = t["external"];
            si["external-filename"] = t["external-filename"];
            si["selected"] = t["selected"];
            si["title"] = t["title"];
            if (t["title"].toString().size() == 0) {
                if (t["lang"].isValid() && t["lang"].toString().size() && t["lang"].toString() != "und")
                    si["title"] = t["lang"];
                else if (!t["external"].toBool())
                    si["title"] = tr("Internal");
            }
            _pmf.subs.append(si);
        }
        ++p;
    }

    qDebug() << _pmf.subs;
    qDebug() << _pmf.audios;
}

void MpvProxy::nextFrame()
{
    if (state() == PlayState::Stopped) return;

    QList<QVariant> args = { "frame-step"};
    my_command(_handle, args);
}

void MpvProxy::previousFrame()
{
    if (state() == PlayState::Stopped) return;

    QList<QVariant> args = { "frame-back-step"};
    my_command(_handle, args);
}

QVariant MpvProxy::getProperty(const QString &name)
{
    return my_get_property(_handle, name.toUtf8().data());
}

void MpvProxy::setProperty(const QString &name, const QVariant &val)
{
    if (name == "pause-on-start") {
        _pauseOnStart = val.toBool();
    } else if (name == "video-zoom") {
        my_set_property(_handle, name, val.toDouble());
    } else {
        my_set_property(_handle, name.toUtf8().data(), val);
    }
}

} // end of namespace dmr

