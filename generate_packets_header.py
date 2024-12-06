#!/bin/env python3
# SPDX-License-Identifier: MIT
#
# SPDX-FileCopyrightText: Copyright (c) 2024 Ian Hangartner <icrashstuff at outlook dot com>
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.
#
# The reason this generator exists is to make it easy to create additional
# functions for simple packets (eg. draw_imgui or something like that)
#
# My definition of simple packets are fixed length and string only var length
from pprint import pprint

JBOOL = "jbool"
JBYTE = "jbyte"
JUBYTE = "jubyte"
JSHORT = "jshort"
JINT = "jint"
JLONG = "jlong"
JFLOAT = "jfloat"
JDOUBLE = "jdouble"
JSTRING16 = "std::string"
JMETADATA = "jmetadata"

jbool = JBOOL
jbyte = JBYTE
jubyte = JUBYTE
jshort = JSHORT
jint = JINT
jlong = JLONG
jfloat = JFLOAT
jdouble = JDOUBLE
jstring16 = JSTRING16
jmetadata = JMETADATA


packet_defs = []

CLIENT_TO_SERVER = 1
SERVER_TO_CLIENT = 2


def add_packet(typename, pack_id, pack_fields, direc=0, cmt=""):
    global packet_defs
    packet_defs.append((typename, pack_id, pack_fields, direc, cmt))


def print_packet(pack):
    s = ""
    if (pack[3] == CLIENT_TO_SERVER):
        s += "/**\n"
        if (len(pack[4]) > 0):
            for i in pack[4].split("\n"):
                s += " * %s\n" % i
            s += "\n"
        s += " * Client -> Server\n"
        s += " */\n"
    elif (pack[3] == SERVER_TO_CLIENT):
        s += "/**\n"
        if (len(pack[4]) > 0):
            for i in pack[4].split("\n"):
                s += " * %s\n" % i
            s += "\n"
        s += " * Server -> Client\n"
        s += " */\n"
    elif (len(pack[4])):
        s += "/**\n"
        for i in pack[4].split("\n"):
            s += " * %s\n" % i
        s += " */\n"

    s += "struct packet_%s_t : packet_t\n{\n\tpacket_%s_t() { id = PACKET_ID_%s; }\n\n" % (
        pack[0], pack[0], pack[1])

    for i in pack[2].keys():
        t = pack[2][i]
        if (t == JSTRING16):
            s += "\t%s %s;\n" % (t, i)
        elif (t == JFLOAT or t == JDOUBLE):
            s += "\t%s %s = 0.0;\n" % (t, i)
        else:
            s += "\t%s %s = 0;\n" % (t, i)

    if (len(pack[2].keys())):
        s += "\n"

    s += """\tstd::vector<Uint8> assemble()
\t{
\t\tstd::vector<Uint8> dat;
\t\tassert(id == PACKET_ID_%s);
\t\tdat.push_back(id);\n""" % pack[1]

    for i in pack[2].keys():
        t = pack[2][i]
        if (t == JBOOL):
            s += "\t\tassemble_%s(dat, %s);\n" % ("bool", i)
        elif (t == JBYTE):
            s += "\t\tassemble_%s(dat, %s);\n" % ("byte", i)
        elif (t == JUBYTE):
            s += "\t\tassemble_%s(dat, %s);\n" % ("ubyte", i)
        elif (t == JSHORT):
            s += "\t\tassemble_%s(dat, %s);\n" % ("short", i)
        elif (t == JINT):
            s += "\t\tassemble_%s(dat, %s);\n" % ("int", i)
        elif (t == JLONG):
            s += "\t\tassemble_%s(dat, %s);\n" % ("long", i)
        elif (t == JFLOAT):
            s += "\t\tassemble_%s(dat, %s);\n" % ("float", i)
        elif (t == JDOUBLE):
            s += "\t\tassemble_%s(dat, %s);\n" % ("double", i)
        elif (t == JSTRING16):
            s += "\t\tassemble_%s(dat, %s);\n" % ("string16", i)
        else:
            print("Unknown type: \"%s\", exiting!" % t)
            exit(1)

    s += "\t\treturn dat;\n\t}\n"

    s += "\n};"
    return s.replace("\t", " " * 4)


def print_vlen_packet(pack, is_server):
    if (is_server and pack[3] == SERVER_TO_CLIENT):
        return ""
    elif (not is_server and pack[3] == CLIENT_TO_SERVER):
        return ""

    var_len = 0
    pos = 1

    s = "\tcase PACKET_ID_%s:\n\t{\n" % pack[1]

    for i in pack[2].keys():
        t = pack[2][i]
        if (var_len > 1):
            return "    // PACKET_ID_%s has a var_len > 1, handle it yourself\n" % pack[1]
        if (t == JBOOL or t == JBYTE or t == JUBYTE):
            pos += 1
        elif (t == JSHORT):
            pos += 2
        elif (t == JINT or t == JFLOAT):
            pos += 4
        elif (t == JLONG or t == JDOUBLE):
            pos += 8
        elif (t == JSTRING16):
            var_len += 1
            s += "\t\tGET_STR_LEN(%d, %d);\n" % (var_len, pos)
        else:
            print("Unknown type: \"%s\", exiting!" % t)
            exit(1)

    if (var_len == 0):
        return ""

    s += "\t\tbreak;\n\t}\n"

    return s.replace("\t", " " * 4)


def print_parse_packet(pack, is_server):
    if (is_server and pack[3] == SERVER_TO_CLIENT):
        return ""
    elif (not is_server and pack[3] == CLIENT_TO_SERVER):
        return ""
    s = "\tcase PACKET_ID_%s:\n" % pack[1]
    s += "\t{\n"
    s += "\t\tP(packet_%s_t)\n" % pack[0]
    if (len(pack[2].keys()) > 0):
        s += "\n"

    for i in pack[2].keys():
        t = pack[2][i]
        if (t == JBOOL):
            s += "\t\terr += !read_%s(buf, pos, &p->%s);\n" % ("ubyte", i)
        elif (t == JBYTE):
            s += "\t\terr += !read_%s(buf, pos, &p->%s);\n" % ("byte", i)
        elif (t == JUBYTE):
            s += "\t\terr += !read_%s(buf, pos, &p->%s);\n" % ("ubyte", i)
        elif (t == JSHORT):
            s += "\t\terr += !read_%s(buf, pos, &p->%s);\n" % ("short", i)
        elif (t == JINT):
            s += "\t\terr += !read_%s(buf, pos, &p->%s);\n" % ("int", i)
        elif (t == JLONG):
            s += "\t\terr += !read_%s(buf, pos, &p->%s);\n" % ("long", i)
        elif (t == JFLOAT):
            s += "\t\terr += !read_%s(buf, pos, &p->%s);\n" % ("float", i)
        elif (t == JDOUBLE):
            s += "\t\terr += !read_%s(buf, pos, &p->%s);\n" % ("double", i)
        elif (t == JSTRING16):
            s += "\t\terr += !read_%s(buf, pos, p->%s);\n" % ("string16", i)
        else:
            print("Unknown type: \"%s\", exiting!" % t)
            exit(1)
    s += "\n"
    s += "\t\tbreak;\n"
    s += "\t}\n"
    return s.replace("\t", " " * 4)


add_packet("keep_alive", "KEEP_ALIVE", {"keep_alive_id": JINT, })
add_packet("login_request_c2s", "LOGIN_REQUEST", {"protocol_ver": JINT,
                                                  "username": JSTRING16,
                                                  "unused0": JLONG,
                                                  "unused1": JINT,
                                                  "unused2": JBYTE,
                                                  "unused3": JBYTE,
                                                  "unused4": JUBYTE,
                                                  "unused5": JUBYTE, }, CLIENT_TO_SERVER)
add_packet("login_request_s2c", "LOGIN_REQUEST", {"player_eid": JINT,
                                                  "unused": JSTRING16,
                                                  "seed": JLONG,
                                                  "mode": JINT,
                                                  "dimension": JBYTE,
                                                  "difficulty": JBYTE,
                                                  "world_height": JUBYTE,
                                                  "max_players": JUBYTE, }, SERVER_TO_CLIENT)
add_packet("handshake_c2s", "HANDSHAKE", {
           "username": JSTRING16}, CLIENT_TO_SERVER)
add_packet("handshake_s2c", "HANDSHAKE", {
           "connection_hash": JSTRING16, }, SERVER_TO_CLIENT)
add_packet("chat_message", "CHAT_MSG", {"msg": JSTRING16})
add_packet("time_update", "UPDATE_TIME", {"time": JLONG})
add_packet("ent_equipment", "ENT_EQUIPMENT", {"eid": jint,
                                              "slot": jshort,
                                              "item_id": jshort,
                                              "damage": jshort, })
add_packet("spawn_pos", "SPAWN_POS", {"x": jint,
                                      "y": jint,
                                      "z": jint, }, SERVER_TO_CLIENT)
add_packet("ent_use", "ENT_USE", {"user": jint,
                                  "target": jint,
                                  "left_click": jbool, })
add_packet("health", "UPDATE_HEALTH", {"health": jshort,
                                       "food": jshort,
                                       "food_saturation": jfloat, })
add_packet("respawn", "RESPAWN", {"dimension": jbyte,
                                  "difficulty": jbyte,
                                  "mode": jbyte,
                                  "world_height": jshort,
                                  "seed": jlong, },
           cmt="Sent by client after hitting respawn\nSent by server to change dimension or as a response to the client")
add_packet("on_ground", "PLAYER_ON_GROUND", {
           "on_ground": jbool}, CLIENT_TO_SERVER)
add_packet("player_pos", "PLAYER_POS", {"x": jdouble,
                                        "y": jdouble,
                                        "stance": jdouble,
                                        "z": jdouble,
                                        "on_ground": jbool, }, CLIENT_TO_SERVER)
add_packet("player_look", "PLAYER_LOOK", {"yaw": jfloat,
                                          "pitch": jfloat,
                                          "on_ground": jbool}, CLIENT_TO_SERVER)
add_packet("player_pos_look_c2s", "PLAYER_POS_LOOK", {"x": jdouble,
                                                      "y": jdouble,
                                                      "stance": jdouble,
                                                      "z": jdouble,
                                                      "yaw": jfloat,
                                                      "pitch": jfloat,
                                                      "on_ground": jbool, }, CLIENT_TO_SERVER)
add_packet("player_pos_look_s2c", "PLAYER_POS_LOOK", {"x": jdouble,
                                                      "stance": jdouble,
                                                      "y": jdouble,
                                                      "z": jdouble,
                                                      "yaw": jfloat,
                                                      "pitch": jfloat,
                                                      "on_ground": jbool, }, SERVER_TO_CLIENT)
add_packet("player_dig", "PLAYER_DIG", {"status": jbyte,
                                        "x": jint,
                                        "y": jbyte,
                                        "z": jint,
                                        "face": jbyte, })

# packet_player_place_t is a complex packet and is therefore omitted

add_packet("hold_change", "HOLD_CHANGE", {"slot_id": jshort})
add_packet("use_bed", "USE_BED", {"eid": jint,
                                  "unknown_probably_in_bed": jbyte,
                                  "headboard_x": jint,
                                  "headboard_y": jbyte,
                                  "headboard_z": jint, })
add_packet("ent_animation", "ENT_ANIMATION", {"eid": jint,
                                              "animate": jbyte})
add_packet("ent_action", "ENT_ACTION", {"eid": jint,
                                        "action_id": jbyte})
add_packet("ent_spawn_named", "ENT_SPAWN_NAMED", {"eid": jint,
                                                  "name": jstring16,
                                                  "x": jint,
                                                  "y": jint,
                                                  "z": jint,
                                                  "rotation": jbyte,
                                                  "pitch": jbyte,
                                                  "cur_item": jshort,
                                                  })
add_packet("ent_spawn_pickup", "ENT_SPAWN_PICKUP", {"eid": jint,
                                                    "item": jshort,
                                                    "count": jbyte,
                                                    "damage": jshort,
                                                    "x": jint,
                                                    "y": jint,
                                                    "z": jint,
                                                    "rotation": jbyte,
                                                    "pitch": jbyte,
                                                    "roll": jbyte,
                                                    })
add_packet("collect_item", "COLLECT_ITEM", {"collected_eid": jint,
                                            "collector_eid": jint, })

# packet_add_obj_t is a complex packet and is therefore omitted

# packet_ent_spawn_mob_t contains metadata, and is currently omitted
# # # add_packet("ent_spawn_mob", "ENT_SPAWN_MOB", { "field":type })

add_packet("ent_spawn_painting", "ENT_SPAWN_PAINTING", {"eid": jint,
                                                        "title": jstring16,
                                                        "center_x": jint,
                                                        "center_y": jint,
                                                        "center_z": jint,
                                                        "direction": jint, })

add_packet("ent_spawn_xp", "ENT_SPAWN_XP", {"eid": jint,
                                            "x": jint,
                                            "y": jint,
                                            "z": jint,
                                            "count": jshort, })

add_packet("stance_update", "STANCE_UPDATE", {"unknown0": jfloat,
                                              "unknown1": jfloat,
                                              "unknown2": jfloat,
                                              "unknown3": jfloat,
                                              "unknown4": jbool,
                                              "unknown5": jbool,
                                              })
add_packet("ent_velocity", "ENT_VELOCITY", {"eid": jint,
                                            "vel_x": jint,
                                            "vel_y": jint,
                                            "vel_z": jint, })

add_packet("ent_destroy", "ENT_DESTROY", {"eid": jint})
add_packet("ent_create", "ENT_ENSURE_SPAWN", {"eid": jint})

add_packet("ent_move_rel", "ENT_MOVE_REL", {"eid": jint,
                                            "delta_x": jbyte,
                                            "delta_y": jbyte,
                                            "delta_z": jbyte, })

add_packet("ent_look", "ENT_LOOK", {"eid": jint,
                                    "yaw": jbyte,
                                    "pitch": jbyte, })

add_packet("ent_look_move_rel", "ENT_LOOK_MOVE_REL", {"eid": jint,
                                                      "delta_x": jbyte,
                                                      "delta_y": jbyte,
                                                      "delta_z": jbyte,
                                                      "yaw": jbyte,
                                                      "pitch": jbyte, })

add_packet("ent_teleport", "ENT_MOVE_TELEPORT", {"eid": jint,
                                                 "x": jint,
                                                 "y": jint,
                                                 "z": jint,
                                                 "rotation": jbyte,
                                                 "pitch": jbyte, })
add_packet("ent_status", "ENT_STATUS", {"eid": jint,
                                        "status": jbyte, })
add_packet("ent_attach", "ENT_ATTACH", {"eid": jint,
                                        "vehicle": jint, })

# packet_ent_metadata_t contains metadata, and is currently omitted
# # # add_packet("ent_metadata", "ENT_METADATA", { "field":type })
add_packet("ent_effect", "ENT_EFFECT", {"eid": jint,
                                        "effect_id": jbyte,
                                        "amplifier": jbyte,
                                        "duration": jshort, })
add_packet("ent_effect_remove", "ENT_EFFECT_REMOVE", {"eid": jint,
                                                      "effect_id": jbyte, })
add_packet("xp_set", "XP_SET", {"current_xp": jbyte,
                                "level": jbyte,
                                "total": jshort, })
add_packet("chunk_cache", "CHUNK_CACHE", {"chunk_x": jint,
                                          "chunk_z": jint,
                                          "mode": jbool, })

# packet_chunk_t is a complex packet and is therefore omitted

# packet_block_change_multi_t is a complex packet and is therefore omitted

add_packet("block_change", "BLOCK_CHANGE", {"block_x":  jint,
                                            "block_y":  jbyte,
                                            "block_z":  jint,
                                            "type":     jbyte,
                                            "metadata": jbyte, })
add_packet("sound_effect", "SFX", {"effect_id":  jint,
                                   "x":          jint,
                                   "y":          jbyte,
                                   "z":          jint,
                                   "sound_data": jint, })
add_packet("new_state", "NEW_STATE", {"reason": jbyte,
                                      "mode": jbyte})
add_packet("thunder", "THUNDERBOLT", {"eid":     jint,
                                      "unknown": jbool,
                                      "x":       jint,
                                      "y":       jint,
                                      "z":       jint, })
add_packet("window_open", "WINDOW_OPEN", {"window_id": jbyte,
                                          "type": jbyte,
                                          "title": jstring16,
                                          "num_slots": jbyte})
add_packet("window_close", "WINDOW_CLOSE", {"window_id": jbyte})

# packet_window_click_t is a complex packet and is therefore omitted

# packet_window_slot_t is a complex packet and is therefore omitted

# packet_window_items_t is a complex packet and is therefore omitted

add_packet("window_update_progress", "WINDOW_UPDATE_PROGRESS", {"window_id": jbyte,
                                                                "progress": jshort,
                                                                "value": jshort}, SERVER_TO_CLIENT)
add_packet("window_transaction", "WINDOW_TRANSACTION", {"window_id": jbyte,
                                                        "action_num": jshort,
                                                        "accepted": jbool})

add_packet("inventory_action_creative", "INV_CREATIVE_ACTION", {"slot": JSHORT,
                                                                "item_id": JSHORT,
                                                                "quantity": JSHORT,
                                                                "damage": JSHORT, })
add_packet("update_sign", "UPDATE_SIGN", {"x": jint,
                                          "y": jshort,
                                          "z": jint,
                                          "text0": jstring16,
                                          "text1": jstring16,
                                          "text2": jstring16,
                                          "text3": jstring16, })

# packet_item_data_t is a complex packet and is therefore omitted

add_packet("increment_statistic", "INCREMENT_STATISTIC", {"stat_id": jint,
                                                          "amount": jbyte, })

add_packet("play_list_item", "PLAYER_LIST_ITEM", {
           "username": JSTRING16, "online": JBOOL, "ping": JSHORT})
add_packet("server_list_ping", "SERVER_LIST_PING", {})
add_packet("kick", "KICK", {"reason": JSTRING16})

header_header = """/* SPDX-License-Identifier: MIT
 *
 * SPDX-FileCopyrightText: Copyright (c) 2024 Ian Hangartner <icrashstuff at outlook dot com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

/* WARNING: This file is automatically generated by generate_packets_header.py, DO NOT EDIT */

#ifndef MCS_B181_PACKET_H
#error "This header should only be included by packet.h"
#endif

#ifndef MCS_B181_PACKET_GEN_DEFS_H
#define MCS_B181_PACKET_GEN_DEFS_H

"""

header_mid = """
#endif
#ifdef MCS_B181_PACKET_GEN_IMPL
#define P(type)          \\
    packet = new type(); \\
    type* p = (type*)packet;

#define GET_STR_LEN(var_len_val, off)                                  \\
    do                                                                 \\
    {                                                                  \\
        if (var_len == var_len_val && buf_size >= ((off) + 2))         \\
        {                                                              \\
            len += (SDL_Swap16BE(*(Uint16*)(buf.data() + (off))) * 2); \\
            var_len--;                                                 \\
            change_happened++;                                         \\
        }                                                              \\
    } while (0)
"""

header_footer = """
#undef P
#undef GET_STR_LEN
#endif
"""

if __name__ == "__main__":
    print(header_header)

    for i in packet_defs:
        print(print_packet(i))

    print(header_mid)

    for j in [("server", True), ("client", False)]:
        print(
            "static bool vlen_gen_%s(Uint8 packet_type, std::vector<Uint8>& buf, size_t& buf_size, int& change_happened, int& var_len, size_t& len)\n{" % j[0])
        print("    switch (packet_type)\n    {")
        for i in packet_defs:
            s = print_vlen_packet(i, j[1])
            if (len(s)):
                print(s)
        print("    default:")
        print("        return 0;")
        print("    }")
        print("    return 1;")
        print("}\n")

        print(
            "static bool parse_gen_packets_%s(Uint8 packet_type, std::vector<Uint8>& buf, int& err, size_t& pos, packet_t*& packet)\n{" % j[0])
        print("    switch (packet_type)\n    {")
        for i in packet_defs:
            s = print_parse_packet(i, j[1])
            if (len(s)):
                print(s)
        print("    default:")
        print("        return 0;")
        print("    }")
        print("    return 1;")
        print("}\n")
    print(header_footer)
