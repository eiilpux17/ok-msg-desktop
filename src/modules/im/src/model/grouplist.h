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

#ifndef GROUPLIST_H
#define GROUPLIST_H

#include "src/model/groupid.h"

template <class A, class B> class QHash;
template <class T> class QList;

class QString;

namespace module::im {
class Group;
using GroupMap = ::QHash<::QString, Group*>;

class GroupList {
public:
    Group* addGroup(Group* g);
    Group* findGroup(const GroupId& groupId) const ;
    const GroupId& id2Key(QString groupNum);
    void removeGroup(const GroupId& groupId, bool fake = false);
    QList<Group*> getAllGroups() const;
    void clear();

private:
    GroupMap groupMap;
};
}  // namespace module::im
#endif  // GROUPLIST_H
