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
/**
 *@file 这个文件是播放音乐时显示的窗口动画
 */
#ifndef MOVIEWIDGET_H
#define MOVIEWIDGET_H

#include <DWidget>
#include <QResizeEvent>

DWIDGET_USE_NAMESPACE

class QTimer;
class QHBoxLayout;
class QLabel;
class QPixmap;

namespace dmr {
/**
 * @brief The MovieWidget class
 * 播放音乐时动画效果显示窗口类
 */
class MovieWidget: public DWidget
{
    Q_OBJECT

    enum PlayState {
        STATE_PLAYING = 0,
        STATE_PAUSE,
        STATE_STOP
    };

public:
    /**
     * @brief MovieWidget 播放动画显示窗口
     * @param parent 父窗口
     */
    explicit MovieWidget(QWidget *parent = nullptr);

public slots:
    /**
     * @brief startPlaying
     * 开始播放时的槽函数
     */
    void startPlaying();
    /**
     * @brief stopPlaying
     * 停止播放时的槽函数
     */
    void stopPlaying();
    /**
     * @brief pausePlaying
     * 暂停播放时的槽函数
     */
    void pausePlaying();
    /**
     * @brief updateView
     * 更新窗口函数
     */
    void updateView();
    /**
     * @brief initMember 初始化成员变量
     */
    void initMember();

private:
    QLabel *m_pLabMovie;          ///显示动画的控件
    QTimer *m_pTimer;             ///控制动画播放速度的时间
    QHBoxLayout *m_pHBoxLayout;   ///窗口布局管理
    QPixmap m_pixmapBg;           ///底下胶片的图像
    QPixmap m_pixmapNote;         ///上层音符的图像
    int m_nRotate;                ///旋转角度
    int m_nWidthNote;             ///音符边长
    PlayState m_state;            ///播放状态
};

}

#endif // MOVIEWIDGET_H
