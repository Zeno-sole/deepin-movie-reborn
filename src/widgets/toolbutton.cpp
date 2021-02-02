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
#include "toolbutton.h"
#include <dthememanager.h>
#include <DApplication>

namespace dmr {
VolumeButton::VolumeButton(QWidget *parent)
    : DIconButton(parent)
{
    changeLevel(Level::High);
    setIcon(QIcon::fromTheme("dcc_volume"));
    setIconSize(QSize(36, 36));
}

void VolumeButton::changeLevel(Level lv)
{
    if (_lv != lv) {
        switch (lv) {
        case Level::Mute:
            setIcon(QIcon::fromTheme("dcc_mute"));
            break;
        case Level::Off:
        case Level::Low:
            setIcon(QIcon::fromTheme("dcc_volumelow"));
            break;
        case Level::Mid:
            setIcon(QIcon::fromTheme("dcc_volumemid"));
            break;
        case Level::High:
            setIcon(QIcon::fromTheme("dcc_volume"));
            break;
        }
        //        setStyleSheet(styleSheet());
        _lv = lv;
    }
}

void VolumeButton::enterEvent(QEvent *ev)
{
    emit entered();

    DIconButton::enterEvent(ev);
}

void VolumeButton::leaveEvent(QEvent *ev)
{
    emit leaved();

    DIconButton::leaveEvent(ev);
}

void VolumeButton::wheelEvent(QWheelEvent *we)
{
    //qDebug() << we->angleDelta() << we->modifiers() << we->buttons();
    if (we->buttons() == Qt::NoButton && we->modifiers() == Qt::NoModifier) {
        if (we->angleDelta().y() > 0) {
            emit requestVolumeUp();
        } else {
            emit requestVolumeDown();
        }
    }
}

}

