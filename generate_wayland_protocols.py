#!/usr/bin/env python3
"""
Minimal wayland-scanner replacement.
Parsles wayland protocol XML and generates C headers with wl_interface structs,
opcode enums, and thin wrapper functions.
"""

import sys
import xml.etree.ElementTree as ET
from pathlib import Path


def type_char(arg_type, interface_name):
    """Map arg type to wayland signature char."""
    mapping = {
        "int": "i",
        "uint": "u",
        "fixed": "f",
        "string": "s",
        "object": "o",
        "new_id": "n",
        "array": "a",
        "fd": "h",
    }
    return mapping.get(arg_type, "?")


def generate_protocol(xml_path, out_h_path):
    tree = ET.parse(xml_path)
    root = tree.getroot()
    protocol_name = root.get("name", "unknown")
    
    lines = []
    lines.append(f"/* Auto-generated from {Path(xml_path).name} -- do not edit. */")
    lines.append("#pragma once")
    lines.append("")
    lines.append('#include <wayland-client.h>')
    lines.append("")
    
    # Forward-declare all interfaces first
    interfaces = root.findall("interface")
    for iface in interfaces:
        name = iface.get("name")
        lines.append(f"extern const struct wl_interface {name}_interface;")
    lines.append("")
    
    for iface in interfaces:
        name = iface.get("name")
        version = iface.get("version", "1")
        requests = iface.findall("request")
        events = iface.findall("event")
        enums = iface.findall("enum")
        
        # Request / event enums
        if requests:
            lines.append(f"enum {name}_request {{")
            for i, req in enumerate(requests):
                req_name = req.get("name").upper()
                lines.append(f"    {name.upper()}_{req_name} = {i},")
            lines.append(f"    {name.upper()}_REQUEST_COUNT = {len(requests)}")
            lines.append("};")
            lines.append("")
        
        if events:
            lines.append(f"enum {name}_event {{")
            for i, ev in enumerate(events):
                ev_name = ev.get("name").upper()
                lines.append(f"    {name.upper()}_{ev_name} = {i},")
            lines.append(f"    {name.upper()}_EVENT_COUNT = {len(events)}")
            lines.append("};")
            lines.append("")
        
        # Enum declarations
        for enum in enums:
            enum_name = enum.get("name").upper()
            lines.append(f"enum {name}_{enum_name} {{")
            for entry in enum.findall("entry"):
                entry_name = entry.get("name").upper()
                value = entry.get("value")
                lines.append(f"    {name.upper()}_{enum_name}_{entry_name} = {value},")
            lines.append("};")
            lines.append("")
        
        # wl_interface method/event tables
        def build_messages(items, suffix):
            if not items:
                return []
            result = [f"static const struct wl_message {name}_{suffix}[] = {{"]
            for item in items:
                item_name = item.get("name")
                sig = ""
                types = []
                for arg in item.findall("arg"):
                    t = arg.get("type", "")
                    sig += type_char(t, arg.get("interface", ""))
                    iface_ref = arg.get("interface")
                    if iface_ref:
                        types.append(f"&{iface_ref}_interface")
                    else:
                        types.append("NULL")
                if types:
                    types_str = ", ".join(types)
                    result.append(f'    {{ "{item_name}", "{sig}", (const struct wl_interface *[]){{ {types_str} }} }},')
                else:
                    result.append(f'    {{ "{item_name}", "{sig}", NULL }},')
            result.append("};")
            result.append("")
            return result
        
        lines.extend(build_messages(requests, "requests"))
        lines.extend(build_messages(events, "events"))
        
        # wl_interface
        req_table = f"{name}_requests" if requests else "NULL"
        ev_table = f"{name}_events" if events else "NULL"
        req_count = len(requests)
        ev_count = len(events)
        lines.append(f"const struct wl_interface {name}_interface = {{")
        lines.append(f'    "{name}", {version},')
        lines.append(f"    {req_count}, {req_table}, {ev_count}, {ev_table}")
        lines.append("};")
        lines.append("")
        
        # Thin wrapper functions for requests
        for req in requests:
            req_name = req.get("name")
            args = req.findall("arg")
            has_new_id = any(a.get("type") == "new_id" for a in args)
            
            if has_new_id:
                # Constructor-style wrapper
                ret_iface = None
                for a in args:
                    if a.get("type") == "new_id":
                        ret_iface = a.get("interface", name)
                        break
                
                params = [f"struct {name} *{name}"]
                marshal_args = [f"(struct wl_proxy*){name}", f"{name.upper()}_{req_name.upper()}", f"&{ret_iface}_interface", "1", "0"]
                for arg in args:
                    if arg.get("type") == "new_id":
                        continue
                    aname = arg.get("name", "arg")
                    atype = arg.get("type")
                    if atype == "object":
                        iface = arg.get("interface", "wl_object")
                        params.append(f"struct {iface} *{aname}")
                        marshal_args.append(f"(struct wl_proxy*){aname}")
                    elif atype == "string":
                        params.append(f"const char *{aname}")
                        marshal_args.append(aname)
                    elif atype == "uint":
                        params.append(f"uint32_t {aname}")
                        marshal_args.append(aname)
                    elif atype == "int":
                        params.append(f"int32_t {aname}")
                        marshal_args.append(aname)
                    elif atype == "fixed":
                        params.append(f"wl_fixed_t {aname}")
                        marshal_args.append(aname)
                    elif atype == "array":
                        params.append(f"struct wl_array *{aname}")
                        marshal_args.append(aname)
                    elif atype == "fd":
                        params.append(f"int32_t {aname}")
                        marshal_args.append(aname)
                    else:
                        params.append(f"void *{aname}")
                        marshal_args.append(aname)
                
                args_str = ", ".join(params)
                margs_str = ", ".join(marshal_args)
                lines.append(f"static inline struct {ret_iface} *{name}_{req_name}({args_str}) {{")
                lines.append(f"    return (struct {ret_iface} *)wl_proxy_marshal_flags(")
                lines.append(f"        {margs_str});")
                lines.append("}")
                lines.append("")
            else:
                # Simple wrapper
                params = [f"struct {name} *{name}"]
                marshal_args = [f"(struct wl_proxy*){name}", f"{name.upper()}_{req_name.upper()}", "NULL", "0", "0"]
                for arg in args:
                    aname = arg.get("name", "arg")
                    atype = arg.get("type")
                    if atype == "object":
                        iface = arg.get("interface", "wl_object")
                        params.append(f"struct {iface} *{aname}")
                        marshal_args.append(f"(struct wl_proxy*){aname}")
                    elif atype == "string":
                        params.append(f"const char *{aname}")
                        marshal_args.append(aname)
                    elif atype == "uint":
                        params.append(f"uint32_t {aname}")
                        marshal_args.append(aname)
                    elif atype == "int":
                        params.append(f"int32_t {aname}")
                        marshal_args.append(aname)
                    elif atype == "fixed":
                        params.append(f"wl_fixed_t {aname}")
                        marshal_args.append(aname)
                    elif atype == "array":
                        params.append(f"struct wl_array *{aname}")
                        marshal_args.append(aname)
                    elif atype == "fd":
                        params.append(f"int32_t {aname}")
                        marshal_args.append(aname)
                    else:
                        params.append(f"void *{aname}")
                        marshal_args.append(aname)
                
                args_str = ", ".join(params)
                margs_str = ", ".join(marshal_args)
                lines.append(f"static inline void {name}_{req_name}({args_str}) {{")
                lines.append(f"    wl_proxy_marshal_flags({margs_str});")
                lines.append("}")
                lines.append("")
        
        # Listener struct and add_listener wrapper
        if events:
            lines.append(f"struct {name}_listener {{")
            for ev in events:
                ev_name = ev.get("name")
                params = [f"void *data", f"struct {name} *{name}"]
                for arg in ev.findall("arg"):
                    aname = arg.get("name", "arg")
                    atype = arg.get("type")
                    if atype == "object":
                        iface = arg.get("interface", "wl_object")
                        params.append(f"struct {iface} *{aname}")
                    elif atype == "string":
                        params.append(f"const char *{aname}")
                    elif atype == "uint":
                        params.append(f"uint32_t {aname}")
                    elif atype == "int":
                        params.append(f"int32_t {aname}")
                    elif atype == "fixed":
                        params.append(f"wl_fixed_t {aname}")
                    elif atype == "array":
                        params.append(f"struct wl_array *{aname}")
                    elif atype == "fd":
                        params.append(f"int32_t {aname}")
                    else:
                        params.append(f"void *{aname}")
                params_str = ", ".join(params)
                lines.append(f"    void (*{ev_name})({params_str});")
            lines.append("};")
            lines.append("")
            lines.append(f"static inline int {name}_add_listener(struct {name} *{name},")
            lines.append(f"    const struct {name}_listener *listener, void *data) {{")
            lines.append(f"    return wl_proxy_add_listener((struct wl_proxy*){name},")
            lines.append(f"        (void (**)(void))listener, data);")
            lines.append("}")
            lines.append("")
    
    with open(out_h_path, "w") as f:
        f.write("\n".join(lines) + "\n")
    print(f"Generated: {out_h_path}")


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python3 generate_wayland_protocols.py <xml_file> [output.h]")
        sys.exit(1)
    
    xml_file = sys.argv[1]
    out_file = sys.argv[2] if len(sys.argv) > 2 else Path(xml_file).stem + "-client-protocol.h"
    generate_protocol(xml_file, out_file)
