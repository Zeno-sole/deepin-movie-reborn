/*
 * Copyright (C) 2020 ~ 2021, Deepin Technology Co., Ltd. <support@deepin.org>
 *
 * Author:     zhuyuliang <zhuyuliang@uniontech.com>
 *
 * Maintainer: xiepengfei <xiepengfei@uniontech.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "volumeslider.h"
#include "toolbox_proxy.h"
#include "dbusutils.h"

DWIDGET_USE_NAMESPACE

namespace dmr {

VolumeSlider::VolumeSlider(MainWindow *mw, QWidget *parent)
    : DArrowRectangle(DArrowRectangle::ArrowBottom, DArrowRectangle::FloatWidget, parent), _mw(mw)
{
#ifdef __mips__
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint);
#elif __aarch64__
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint);
#elif __sw_64__
    setWindowFlags(Qt::FramelessWindowHint | Qt::BypassWindowManagerHint);
    setAttribute(Qt::WA_NativeWindow);
#elif __x86_64__
    if (!CompositingManager::get().composited()) {
        setWindowFlags(Qt::Tool | Qt::FramelessWindowHint);
    }
#endif
    m_iStep = 0;
    m_bIsWheel = false;
    m_nVolume = 100;
    m_bHideWhenFinished = false;

    hide();
    setFocusPolicy(Qt::TabFocus);
    QVBoxLayout *vLayout = new QVBoxLayout(this);
    vLayout->setContentsMargins(2, 16, 2, 14);  //内边距，与UI沟通确定
    vLayout->setSpacing(0);
    setLayout(vLayout);

    QFont font;
    font.setFamily("SourceHanSansSC");
    font.setWeight(QFont::Medium);

    vLayout->addStretch();
    m_pLabShowVolume = new DLabel(_mw);
    DFontSizeManager::instance()->bind(m_pLabShowVolume, DFontSizeManager::T6);
    m_pLabShowVolume->setForegroundRole(QPalette::BrightText);
    m_pLabShowVolume->setFont(font);
    m_pLabShowVolume->setAlignment(Qt::AlignCenter);
    m_pLabShowVolume->setText("0%");
    vLayout->addWidget(m_pLabShowVolume, 0, Qt::AlignCenter);

    m_slider = new DSlider(Qt::Vertical, this);
    m_slider->setFixedWidth(24);
    m_slider->setIconSize(QSize(15, 15));
    m_slider->installEventFilter(this);
    m_slider->show();
    m_slider->slider()->setRange(0, 100);
    m_slider->setValue(m_nVolume);
    m_slider->setObjectName(VOLUME_SLIDER);
    m_slider->slider()->setObjectName(SLIDER);
    m_slider->slider()->setAccessibleName(SLIDER);
    m_slider->slider()->setMinimumHeight(120);
    m_slider->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred));
    m_slider->slider()->setFocusPolicy(Qt::NoFocus);
    vLayout->addWidget(m_slider, 1, Qt::AlignCenter);

    m_pBtnChangeMute = new DToolButton(this);
    m_pBtnChangeMute->setObjectName(MUTE_BTN);
    m_pBtnChangeMute->setAccessibleName(MUTE_BTN);
    m_pBtnChangeMute->setToolButtonStyle(Qt::ToolButtonIconOnly);
    //同时设置按钮与图标的尺寸，改变其默认比例
    m_pBtnChangeMute->setFixedSize(30, 30);
    m_pBtnChangeMute->setIconSize(QSize(30, 30));
    m_pBtnChangeMute->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred));
    m_pBtnChangeMute->setFocusPolicy(Qt::NoFocus);
    vLayout->addStretch(10);
    vLayout->addWidget(m_pBtnChangeMute, 0, Qt::AlignHCenter);
    vLayout->addStretch();

    connect(m_slider, &DSlider::valueChanged, this, &VolumeSlider::volumeChanged);
    connect(m_pBtnChangeMute, SIGNAL(clicked()), this, SLOT(muteButtnClicked()));
    connect(DGuiApplicationHelper::instance(), &DGuiApplicationHelper::themeTypeChanged,
            this, &VolumeSlider::setThemeType);
    m_autoHideTimer.setSingleShot(true);
#ifdef __x86_64__
    connect(&m_autoHideTimer, &QTimer::timeout, this, &VolumeSlider::popup);
#else
    if (utils::check_wayland_env()) {
        connect(&m_autoHideTimer, &QTimer::timeout, this, &VolumeSlider::hide);
    }
#endif

//与dock栏音量的dbus通信接口暂时注释
//    ThreadPool::instance()->moveToNewThread(&volumeMonitoring);
//    volumeMonitoring.start();
//    connect(&volumeMonitoring, &VolumeMonitoring::volumeChanged, this, &VolumeSlider::changeVolume);
//    connect(&volumeMonitoring, &VolumeMonitoring::muteChanged, this, &VolumeSlider::changeMuteState);
}

VolumeSlider::~VolumeSlider()
{
}

void VolumeSlider::initVolume()
{
    QTimer::singleShot(100, this, [ = ] { //延迟加载等待信号槽连接
        int nVolume = Settings::get().internalOption("global_volume").toInt();
        bool bMute = Settings::get().internalOption("mute").toBool();

        changeVolume(nVolume);
        changeMuteState(bMute);
    });
}

void VolumeSlider::stopTimer()
{
    m_autoHideTimer.stop();
}

QString VolumeSlider::readSinkInputPath()
{
    QString strPath = "";

    QVariant v = ApplicationAdaptor::redDBusProperty("com.deepin.daemon.Audio", "/com/deepin/daemon/Audio",
                                                     "com.deepin.daemon.Audio", "SinkInputs");

    if (!v.isValid())
        return strPath;

    QList<QDBusObjectPath> allSinkInputsList = v.value<QList<QDBusObjectPath> >();

    for (auto curPath : allSinkInputsList) {
        QVariant nameV = ApplicationAdaptor::redDBusProperty("com.deepin.daemon.Audio", curPath.path(),
                                                             "com.deepin.daemon.Audio.SinkInput", "Name");
        QString strMovie = QObject::tr("Movie");
        if (!nameV.isValid() || (!nameV.toString().contains(strMovie, Qt::CaseInsensitive) && !nameV.toString().contains("deepin-movie", Qt::CaseInsensitive)))
            continue;

        strPath = curPath.path();
        break;
    }
    return strPath;
}

//    cppckeck修改
//void VolumeSlider::setAudioVolume(int volume)
//{
//    double tVolume = 0.0;
//    if (volume == 100) {
//        tVolume = (volume) / 100.0 ;
//    } else if (volume != 0) {
//        tVolume = (volume + 1) / 100.0 ;
//    }

//    QString sinkInputPath = readSinkInputPath();

//    if (!sinkInputPath.isEmpty()) {
//        QDBusInterface ainterface("com.deepin.daemon.Audio", sinkInputPath,
//                                  "com.deepin.daemon.Audio.SinkInput", QDBusConnection::sessionBus());
//        if (!ainterface.isValid()) {
//            return;
//        }
//        //调用设置音量
//        ainterface.call(QLatin1String("SetVolume"), tVolume, false);

//        if (qFuzzyCompare(tVolume, 0.0))
//            ainterface.call(QLatin1String("SetMute"), true);

//        //获取是否静音
//        QVariant muteV = ApplicationAdaptor::redDBusProperty("com.deepin.daemon.Audio", sinkInputPath,
//                                                             "com.deepin.daemon.Audio.SinkInput", "Mute");
//    }
//}

void VolumeSlider::setMute(bool muted)
{
    QString sinkInputPath = readSinkInputPath();

    if (!sinkInputPath.isEmpty()) {
        QDBusInterface ainterface("com.deepin.daemon.Audio", sinkInputPath,
                                  "com.deepin.daemon.Audio.SinkInput",
                                  QDBusConnection::sessionBus());
        if (!ainterface.isValid()) {
            return;
        }

        //调用设置音量
        ainterface.call(QLatin1String("SetMute"), muted);
    }

    return;
}

void VolumeSlider::updatePoint(QPoint point)
{
    QRect main_rect = _mw->rect();
    QRect view_rect = main_rect.marginsRemoved(QMargins(1, 1, 1, 1));
    m_point = point + QPoint(view_rect.width() - (TOOLBOX_BUTTON_WIDTH * 2 + 30 + (VOLSLIDER_WIDTH - TOOLBOX_BUTTON_WIDTH) / 2),
                             view_rect.height() - TOOLBOX_HEIGHT - VOLSLIDER_HEIGHT);
}
void VolumeSlider::popup()
{
    QRect main_rect = _mw->rect();
    QRect view_rect = main_rect.marginsRemoved(QMargins(1, 1, 1, 1));

    int x = view_rect.width() - (TOOLBOX_BUTTON_WIDTH * 2 + 30 + (VOLSLIDER_WIDTH - TOOLBOX_BUTTON_WIDTH) / 2);
    int y = view_rect.height() - TOOLBOX_HEIGHT - VOLSLIDER_HEIGHT;
#ifndef __x86_64__
    //在arm及mips平台下音量条上移了10个像素
    y += 10;
#endif
    QRect end(x, y, VOLSLIDER_WIDTH, VOLSLIDER_HEIGHT);
    QRect start = end;

    start.setWidth(start.width() + 12);
    start.setHeight(start.height() + 10);
#ifdef __x86_64__
    if(CompositingManager::get().composited()) {
        start.moveTo(start.topLeft() - QPoint(6, 10));
    } else {
        end.moveTo(m_point + QPoint(6, 0));
        start.moveTo(m_point - QPoint(0, 14));
    }
#elif __sw_64__
    start.moveTo(start.topLeft() - QPoint(6, 10));
#else
    end.moveTo(m_point + QPoint(6, 0));
    start.moveTo(m_point - QPoint(0, 14));
#endif

    //动画未完成，等待动画结束后再隐藏控件
    if (pVolAnimation) {
        m_bHideWhenFinished = true;
        return;
    }

    if (m_state == State::Close && isVisible()) {
        pVolAnimation = new QPropertyAnimation(this, "geometry");
        pVolAnimation->setEasingCurve(QEasingCurve::Linear);
        pVolAnimation->setKeyValueAt(0, end);
        pVolAnimation->setKeyValueAt(0.3, start);
        pVolAnimation->setKeyValueAt(1, end);
        pVolAnimation->setDuration(230);
        m_bFinished = true;
        raise();
        pVolAnimation->start();
        connect(pVolAnimation, &QPropertyAnimation::finished, [ = ] {
            pVolAnimation->deleteLater();
            pVolAnimation = nullptr;
            m_state = Open;
            m_bFinished = false;
            if (m_bHideWhenFinished) {
                popup();
                m_bHideWhenFinished = false;
            }
        });
    } else {
        m_state = Close;
        hide();
    }
}
void VolumeSlider::delayedHide()
{
#ifdef __x86_64__
    if (!isHidden())
        m_autoHideTimer.start(500);
#else
    m_mouseIn = false;
    DUtil::TimerSingleShot(100, [this]() {
        if (!m_mouseIn)
            popup();
    });
#endif
}
void VolumeSlider::changeVolume(int nVolume)
{
    if (nVolume <= 0) {
        nVolume = 0;
    } else if (nVolume > 200) {
        nVolume = 200;
    }

    m_nVolume = nVolume;

    m_slider->setValue(nVolume > 100 ? 100 : nVolume);  //音量实际能改变200,但是音量条最大支持到100

    //1.保证初始化音量(100)和配置文件中音量一致时，也能得到刷新
    //2.m_slider的最大值为100,如果大于100必须主动调用
    if (nVolume >= 100) {
        volumeChanged(nVolume);
    }
}

void VolumeSlider::calculationStep(int iAngleDelta){
    //int wheelSpeed;
    m_bIsWheel = true;
    //获取系统鼠标滚轮灵敏度
    //QVariant v = DBusUtils::redDBusProperty("com.deepin.daemon.InputDevices", "/com/deepin/daemon/InputDevices","com.deepin.daemon.InputDevices", "WheelSpeed");
    //if (!v.isValid()){
    //    wheelSpeed = 1;
    //}else{
    //    wheelSpeed = v.toInt();
    //    //wheelSpeed = 1;
    //}
    //iAngleDelta = iAngleDelta/wheelSpeed;

    if ((m_iStep > 0 && iAngleDelta > 0) || (m_iStep < 0 && iAngleDelta < 0)) {
        m_iStep += iAngleDelta;
    } else {
        m_iStep = iAngleDelta;
    }
}

void VolumeSlider::volumeUp()
{
    if (m_bIsWheel) {
        if(qAbs(m_iStep) >= 120) {
            m_nVolume += qAbs(m_iStep) / 120 * 10;
            changeVolume(qMin(m_nVolume, 200));
            m_iStep = 0;
        }
    } else {
        changeVolume(qMin(m_nVolume + 10, 200));
    }
}

void VolumeSlider::volumeDown()
{
    if(m_bIsWheel){
        if(qAbs(m_iStep) >= 120){
            m_nVolume -= qAbs(m_iStep) / 120 * 10 ;
            changeVolume(qMax(m_nVolume, 0));
            m_iStep = 0;
        }
    }else{
        changeVolume(qMax(m_nVolume - 10, 0));
    }

}

void VolumeSlider::changeMuteState(bool bMute)
{
    //disconnect(&volumeMonitoring, &VolumeMonitoring::muteChanged, this, &VolumeSlider::changeMuteState);

    if (m_bIsMute == bMute || m_nVolume == 0) {
        return;
    }

    //setMute(bMute);
    m_bIsMute = bMute;
    refreshIcon();
    Settings::get().setInternalOption("mute", m_bIsMute);

    emit sigMuteStateChanged(bMute);

    //connect(&volumeMonitoring, &VolumeMonitoring::muteChanged, this, &VolumeSlider::changeMuteState);
}

void VolumeSlider::volumeChanged(int nVolume)
{
    //disconnect(&volumeMonitoring, &VolumeMonitoring::volumeChanged, this, &VolumeSlider::changeVolume);

    if (m_nVolume != nVolume) {
        m_nVolume = nVolume;
    }

    if (m_nVolume > 0 && m_bIsMute) {      //音量改变时改变静音状态
        changeMuteState(false);
    }

    refreshIcon();

    Settings::get().setInternalOption("global_volume", m_nVolume > 100 ? 100 : m_nVolume);

    emit sigVolumeChanged(nVolume);

    //setAudioVolume(nVolume);

    //connect(&volumeMonitoring, &VolumeMonitoring::volumeChanged, this, &VolumeSlider::changeVolume);
}

void VolumeSlider::refreshIcon()
{
    int nValue = m_slider->value();

    if (nValue >= 66)
        m_pBtnChangeMute->setIcon(QIcon::fromTheme("dcc_volume"));
    else if (nValue >= 33)
        m_pBtnChangeMute->setIcon(QIcon::fromTheme("dcc_volumemid"));
    else
        m_pBtnChangeMute->setIcon(QIcon::fromTheme("dcc_volumelow"));

    if (m_bIsMute || nValue == 0)
        m_pBtnChangeMute->setIcon(QIcon::fromTheme("dcc_mute"));

    m_pLabShowVolume->setText(QString("%1%").arg(nValue * 1.0 / m_slider->maximum() * 100));
}

void VolumeSlider::muteButtnClicked()
{
    changeMuteState(!m_bIsMute);
}

bool VolumeSlider::getsliderstate()
{
    return m_bFinished;
}

int VolumeSlider::getVolume()
{
    return m_nVolume;
}

void VolumeSlider::setThemeType(int type)
{
    Q_UNUSED(type)
}

void VolumeSlider::enterEvent(QEvent *e)
{
#ifdef __x86_64__
    m_autoHideTimer.stop();
#else
    m_mouseIn = true;
    QWidget::leaveEvent(e);
#endif
}
void VolumeSlider::showEvent(QShowEvent *se)
{
#ifdef __x86_64__
    m_autoHideTimer.stop();
#else
    QWidget::showEvent(se);
#endif
    QWidget::showEvent(se);
}
void VolumeSlider::leaveEvent(QEvent *e)
{
#ifdef __x86_64__
    m_autoHideTimer.start(500);
#else
    m_mouseIn = false;
    delayedHide();
    QWidget::leaveEvent(e);
#endif
}
void VolumeSlider::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    QColor bgColor = this->palette().background().color();

#if defined (__mips__) || defined (__aarch64__)
    ///arm和mips下控件圆角显示有黑边,在此重绘
    painter.fillRect(rect(), bgColor);
#else
    if (!CompositingManager::get().composited()) {
        painter.fillRect(rect(), bgColor);
    } else {
        double dRation = this->height() * 1.0 / VOLSLIDER_HEIGHT;
        const qreal radius = 20 * dRation;
        const qreal triHeight = 30 * dRation;
        const qreal height = this->height() - triHeight;
        const qreal width = this->width();

        painter.setRenderHints(QPainter::Antialiasing | QPainter::HighQualityAntialiasing);

        // 背景上矩形
        //这里要一次绘制不然中间会出现虚线
        QPainterPath pathRect;
        pathRect.moveTo(radius, 0);
        pathRect.lineTo(width - radius, 0);
        pathRect.arcTo(QRectF(QPointF(width - 2 * radius, 0), QPointF(width, 2 * radius)), 90.0, -90.0);
        pathRect.lineTo(width, height);
        pathRect.lineTo(0, height);
        pathRect.lineTo(0, radius);
        pathRect.arcTo(QRectF(QPointF(0, 0), QPointF(2 * radius, 2 * radius)), 180.0, -90.0);

        // 背景下三角，半边
        qreal radius1 = radius / 2;
        QPainterPath pathTriangle;
        pathTriangle.moveTo(0, height - radius1);
        pathTriangle.arcTo(QRectF(QPointF(0, height - radius1), QSizeF(2 * radius1, 2 * radius1)), 180, 60);
        pathTriangle.lineTo(width / 2, this->height());
        qreal radius2 = radius / 4;
        pathTriangle.arcTo(QRectF(QPointF(width / 2 - radius2, this->height() - radius2 * 2 - 2), QSizeF(2 * radius2, 2 * radius2)), 220, 130);
        pathTriangle.lineTo(width / 2, height);

        // 正向绘制
        painter.fillPath(pathRect, bgColor);
        painter.fillPath(pathTriangle, bgColor);

        // 平移坐标系
        painter.translate(width, 0);
        // 坐标系X反转
        painter.scale(-1, 1);

        // 反向绘制
        painter.fillPath(pathTriangle, bgColor);
    }
#endif
}

void VolumeSlider::keyPressEvent(QKeyEvent *pEvent)
{
#if defined (__mips__) || defined (__aarch64__)     // mips和arm模式下,键盘交互升起音量条焦点在音量条,所以键盘事件被音量条截获
    int nCurVolume = getVolume();
    if (pEvent->key() == Qt::Key_Up) {
        changeVolume(qMin(nCurVolume + 5, 200));
    } else if (pEvent->key() == Qt::Key_Down) {
        changeVolume(qMax(nCurVolume - 5, 0));
    }
#endif

    DArrowRectangle::keyPressEvent(pEvent);
}

bool VolumeSlider::eventFilter(QObject *obj, QEvent *e)
{
    if (e->type() == QEvent::Wheel) {
        QWheelEvent *we = static_cast<QWheelEvent *>(e);
        qInfo() << we->angleDelta() << we->modifiers() << we->buttons();
        if (we->buttons() == Qt::NoButton && we->modifiers() == Qt::NoModifier) {
            calculationStep(we->angleDelta().y());
            if (we->angleDelta().y() > 0 ) {
                volumeUp();
            } else {
                volumeDown();
            }
        }
        return false;
    } else {
        return QObject::eventFilter(obj, e);
    }
}

}
