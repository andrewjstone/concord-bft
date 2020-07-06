import sys
import os

sys.path.append(os.path.abspath(".."))

from exceptions import CmfParseError

import semantics


def is_int(type):
    return type in [
        "uint8",
        "uint16",
        "uint32",
        "uint64",
        "int8",
        "int16",
        "int32",
        "int64",
    ]


def typestr(type):
    if not isinstance(type, dict):
        # A primitive type
        if is_int(type):
            return type + "_t"
        if type == "string":
            return "std::string"
        if type == "bytes":
            return "std::vector<uint8_t>"
        return type
    elif "list" in type:
        return "std::vector<" + typestr(type.list.type) + ">"
    elif "kvpair" in type:
        return (
            "std::pair<"
            + typestr(type.kvpair.key)
            + ", "
            + typestr(type.kvpair.value)
            + ">"
        )
    elif "map" in type:
        return (
            "std::map<" + typestr(type.map.key) + ", " + typestr(type.map.value) + ">"
        )
    elif "optional" in type:
        return "std::optional<" + typestr(type.optional.type) + ">"
    elif "oneof" in type:
        return "std::variant<" + ", ".join(type.oneof.msg_names) + ">"
    raise CmfParseError(type.parseinfo, "Invalid field type")


def structstr(msg):
    """ Take a msg AST node and create a string containing source of a C++ struct """
    struct = """
struct {} {{
  static constexpr uint32_t id = {}; \n\n""".format(
        msg.name, msg.id
    )
    for field in msg.fields:
        struct += "  " + typestr(field.type)
        struct += " {}; \n".format(field.name)

    struct += "};"
    return struct


def equalopstr(msg):
    """ Take a msg AST and create an `operator==` functon for the generated struct """
    s = """
bool operator==(const {}& l, const {}& r) {{
  return """.format(
        msg.name, msg.name
    )
    s += (
        " && ".join(
            ["l.{} == r.{}".format(field.name, field.name) for field in msg.fields]
        )
        + ";\n}"
    )
    return s


def serializeVariantStr(oneof_type):
    """
    Take a type dict containing a oneof and create a string containing C++ serialization code for its corresponding variant.
    """
    return """
void Serialize(std::vector<uint8_t>& output, const {}& val) {{
  std::visit([&output](auto&& arg){{
    cmf::Serialize(output, arg.id);
    Serialize(output, arg);
  }}, val);
}}""".format(
        typestr(oneof_type)
    )


def deserializeVariantStr(oneof_type, msgmap):
    """
    Take a type dict containing a oneof and create a string containing C++ serialization code for its corresponding variant.
    """
    s = """
void Deserialize(uint8_t*& start, const uint8_t* end, {}& val) {{
  uint32_t id;
  cmf::Deserialize(start, end, id);
""".format(
        typestr(oneof_type)
    )

    for type_name in oneof_type.oneof.msg_names:
        s += """
  if (id == {}) {{
    {} value;
    Deserialize(start, end, value);
    val = value;
    return;
  }}

""".format(
            msgmap[type_name].id, type_name
        )
    s += """  throw cmf::DeserializeError(std::string("Invalid Message id in variant: ") + std::to_string(id));
}"""
    return s


def serializeOneofsStr(msg):
    """
    Return a string containing C++ serialization code for any oneofs in `msg`
    """
    return "" + "\n\n".join(
        [
            serializeVariantStr(field.type)
            for field in msg.fields
            if "oneof" in field.type
        ]
    )


def deserializeOneofsStr(msg, msgmap):
    """
    Return a string containing C++ deserialization code for any oneofs in `msg`
    """
    return "" + "\n\n".join(
        [
            deserializeVariantStr(field.type, msgmap)
            for field in msg.fields
            if "oneof" in field.type
        ]
    )


def serializestr(msg, msgmap):
    """
    Take a msg AST node and create a string containing a C++ serialization function for the given msg
    """

    s = serializeOneofsStr(msg)
    s += """
void Serialize(std::vector<uint8_t>& output, const {}& t) {{\n""".format(
        msg.name
    )
    for field in msg.fields:
        # Messages and oneofs are generated in the user namespace, so we don't use the `cmf`
        # namespace for them
        if "oneof" in field.type or str(field.type) in msgmap:
            s += "  Serialize(output, t.{});\n".format(field.name)
        else:
            s += "  cmf::Serialize(output, t.{});\n".format(field.name)
    s += "}"
    return s


def deserializestr(msg, msgmap):
    """
    Take a msg AST node and create a string containing a C++ deserialization function for the given msg
    """
    s = deserializeOneofsStr(msg, msgmap)
    s += """
void Deserialize(uint8_t*& input, const uint8_t* end, {}& t) {{
""".format(
        msg.name
    )
    for field in msg.fields:
        # Messages and oneofs are generated in the user namespace, so we don't use the `cmf`
        # namespace for them
        if "oneof" in field.type or str(field.type) in msgmap:
            s += "  Deserialize(input, end, t.{});\n".format(field.name)
        else:
            s += "  cmf::Deserialize(input, end, t.{});\n".format(field.name)
    s += "}"

    # Add a high level entrypoint for the given message.
    s += """

void Deserialize(const std::vector<uint8_t>& input, {}& t) {{
    auto* begin = const_cast<uint8_t*>(input.data());
    Deserialize(begin, begin + input.size(), t);
}}""".format(msg.name)

    return s


def file_header(namespace):
    header = """/***************************************
 Autogenerated by cmfc. Do not modify.
***************************************/

#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "serialize.h"

"""

    if namespace:
        header += "namespace {} {{\n".format(namespace)
    return header


def file_trailer(namespace):
    if namespace:
        return "\n}} // namespace {}\n".format(namespace)
    return "\n"


def translate(ast, namespace=None):
    """
    Walk concord message format(CMF) AST and generate C++ code.

    Return C++ code as a string along with a dict containing all message names mapped to their ast node
    """
    s = file_header(namespace)
    msgmap = dict()
    for msg in ast.msgs:
        msgmap[msg.name] = msg
        s += structstr(msg) + "\n"
        s += equalopstr(msg) + "\n"
        s += serializestr(msg, msgmap) + "\n"
        s += deserializestr(msg, msgmap) + "\n"
    return s + file_trailer(namespace), msgmap
