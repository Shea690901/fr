/*
 * uid.c
 *
 * Created by: Erik Kay
 * Description: new uid / euid data structures and security
 * Modification:
 *   11-1-92 - Erik Kay - initial creation
 *   94.07.14 - Robocoder - replaced linked list with AVL tree, and
 *                made uids into shared strings
 */

#include "config.h"
#include <stdio.h>
#ifdef __386BSD__
#include <string.h>
#endif
#include "lint.h"
#include "avltree.h"
#include "uid.h"

tree *uids = NULL;
userid_t *backbone_uid = NULL;
userid_t *root_uid = NULL;

int uidcmp(uid1, uid2)
    userid_t *uid1;
    userid_t *uid2;
{
    register char *name1, *name2;

    name1 = uid1->name; name2 = uid2->name;
    return (name1 < name2 ? -1 : (name1 > name2 ? 1 : 0));
}

userid_t *add_uid(name)
    char *name;
{
    userid_t *uid, t_uid;
    char *sname;

    sname = make_shared_string(name);
    t_uid.name = sname;
    uid = (userid_t *)tree_srch(uids, uidcmp, &t_uid);
    if (!uid) {
        uid = DMALLOC( sizeof(userid_t), 119, "add_uid");
        uid->name = sname;
        tree_add(&uids, uidcmp, uid, NULL);
    }
    return uid;
}

userid_t *set_root_uid(name)
    char *name;
{
    if (!root_uid)
        return root_uid = add_uid(name);

    tree_delete(&uids, uidcmp, root_uid, NULL);
    root_uid->name = make_shared_string(name);
    tree_add(&uids, uidcmp, root_uid, NULL);
    return root_uid;
}

userid_t *set_backbone_uid(name)
    char *name;
{
    if (!backbone_uid)
        return backbone_uid = add_uid(name);

    tree_delete(&uids, uidcmp, backbone_uid, NULL);
    backbone_uid->name = make_shared_string(name);
    tree_add(&uids, uidcmp, backbone_uid, NULL);
    return backbone_uid;
}
