/*
 * Copyright (c) 2013-2021 Thomas Isaac Lightburn
 *
 *
 * This file is part of OpenKJ.
 *
 * OpenKJ is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "okjtypes.h"
#include <QSqlQuery>
#include <QSqlError>
#include <utility>
#include <spdlog/spdlog.h>

std::ostream & operator<<(std::ostream& os, const okj::RotationSinger& s)
{
    return os
        << " RotationSinger(id: " << s.id
        << " position: " << s.position
        << " name: " << s.name.toStdString()
        << " regular: " << s.regular
        << " addTS: " << s.addTs.toString().toStdString()
        << " valid: " << s.valid
        << ")";
}

std::ostream & operator<<(std::ostream& os, const QString& s)
{
    return os << s.toStdString();
}


namespace okj {



    RotationSinger::RotationSinger() {
        m_logger = spdlog::get("logger");
        m_settings = std::make_shared<Settings>();
    }

    RotationSinger::RotationSinger(int id, QString name, int position, bool regular, QDateTime addTs, bool valid)
            : id(id), name(std::move(name)), position(position), regular(regular), addTs(std::move(addTs)),
              valid(valid) {
        m_logger = spdlog::get("logger");
        m_settings = std::make_shared<Settings>();
    }

    RotationSinger::RotationSinger(const RotationSinger &r1)
            : id(r1.id), name(r1.name), position(r1.position), regular(r1.regular), addTs(r1.addTs), valid(r1.valid) {
        m_logger = spdlog::get("logger");
        m_settings = std::make_shared<Settings>();
    }


}
