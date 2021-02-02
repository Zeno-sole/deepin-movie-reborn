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
#include "dmr_lineedit.h"

namespace dmr {

LineEdit::LineEdit(QWidget *parent)
    : QLineEdit(parent)
{
    setFixedHeight(30);
//    setStyleSheet(R"(
//  QLineEdit {
//        font-size: 11px;
//      border-radius: 3px;
//      background-color: #ffffff;
//      border: 1px solid rgba(0, 0, 0, 0.08);
//        color: #303030;
//  }
//    )");

    QIcon icon;
    icon.addFile(":/resources/icons/input_clear_normal.svg", QSize(), QIcon::Normal);
    icon.addFile(":/resources/icons/input_clear_press.svg", QSize(), QIcon::Selected);
    icon.addFile(":/resources/icons/input_clear_hover.svg", QSize(), QIcon::Active);
    _clearAct = new QAction(icon, "", this);

    connect(_clearAct, &QAction::triggered, this, &QLineEdit::clear);
	connect(this, &QLineEdit::textChanged, this, &LineEdit::slotTextChanged);

}

void LineEdit::showEvent(QShowEvent *se)
{
    QLineEdit::showEvent(se);
}

void LineEdit::resizeEvent(QResizeEvent *re)
{
    QLineEdit::resizeEvent(re);
}

void LineEdit::slotTextChanged(const QString &s)
{
    if (s.isEmpty()) {
        removeAction(_clearAct);
    } else {
        addAction(_clearAct, QLineEdit::TrailingPosition);
    }
}

}
