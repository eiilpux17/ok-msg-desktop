/*
 * Copyright (c) 2022 船山信息 chuanshaninfo.com
 * The project is licensed under Mulan PubL v2.
 * You can use this software according to the terms and conditions of the Mulan
 * PubL v2. You may obtain a copy of Mulan PubL v2 at:
 *          http://license.coscl.org.cn/MulanPubL-2.0
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PubL v2 for more details.
 */

#include "grouplist.h"
#include <QDebug>
#include <QHash>
#include "src/model/group.h"

namespace module::im {

Group* GroupList::addGroup(Group* group) {
    qDebug() << __func__ << "groupId" << group->getId();

    auto groupId = group->getId().toString();
    auto checker = groupMap.value(groupId);
    if (checker) {
        qWarning() << "addGroup: groupId already taken";
        return checker;
    }

    groupMap[groupId] = group;
    return group;
}

Group* GroupList::findGroup(const GroupId& groupId) const {
    return groupMap.value(groupId.toString());
}

void GroupList::removeGroup(const GroupId& groupId, bool /*fake*/) {
    auto g_it = groupMap.find(groupId.toString());
    if (g_it != groupMap.end()) {
        delete *g_it;
        groupMap.erase(g_it);
    }
}

QList<Group*> GroupList::getAllGroups() const {
    return groupMap.values();
}

void GroupList::clear() {
    for (auto groupptr : groupMap) delete groupptr;
    groupMap.clear();
}
}  // namespace module::im
