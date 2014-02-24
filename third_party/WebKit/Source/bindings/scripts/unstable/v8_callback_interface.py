# Copyright (C) 2013 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""Generate template values for a callback interface.

FIXME: Not currently used in build.
This is a rewrite of the Perl IDL compiler in Python, but is not complete.
Once it is complete, we will switch all IDL files over to Python at once.
Until then, please work on the Perl IDL compiler.
For details, see bug http://crbug.com/239771
"""

from v8_globals import includes
from v8_types import cpp_type, cpp_value_to_v8_value, add_includes_for_type
from v8_utilities import v8_class_name, extended_attribute_value_contains

CALLBACK_INTERFACE_H_INCLUDES = set([
    'bindings/v8/ActiveDOMCallback.h',
    'bindings/v8/DOMWrapperWorld.h',
    'bindings/v8/ScopedPersistent.h',
])
CALLBACK_INTERFACE_CPP_INCLUDES = set([
    'core/dom/ExecutionContext.h',
    'bindings/v8/V8Binding.h',
    'bindings/v8/V8Callback.h',
    'wtf/Assertions.h',
])
CPP_TO_V8_CONVERSION = 'v8::Handle<v8::Value> {name}Handle = {cpp_value_to_v8_value};'


def cpp_to_v8_conversion(idl_type, name):
    # Includes handled in includes_for_operation
    this_cpp_value_to_v8_value = cpp_value_to_v8_value(idl_type, name, isolate='isolate')
    return CPP_TO_V8_CONVERSION.format(name=name, cpp_value_to_v8_value=this_cpp_value_to_v8_value)


def generate_callback_interface(callback_interface):
    includes.clear()
    includes.update(CALLBACK_INTERFACE_CPP_INCLUDES)

    methods = [generate_method(operation) for operation in callback_interface.operations]
    template_contents = {
        'cpp_class_name': callback_interface.name,
        'v8_class_name': v8_class_name(callback_interface),
        'header_includes': CALLBACK_INTERFACE_H_INCLUDES,
        'methods': methods,
    }
    return template_contents


def add_includes_for_operation(operation):
    add_includes_for_type(operation.idl_type)
    for argument in operation.arguments:
        add_includes_for_type(argument.idl_type)


def generate_method(operation):
    if operation.idl_type != 'boolean':
        raise Exception("We don't yet support callbacks that return non-boolean values.")
    is_custom = 'Custom' in operation.extended_attributes
    if not is_custom:
        add_includes_for_operation(operation)
    extended_attributes = operation.extended_attributes
    call_with = extended_attributes.get('CallWith')
    contents = {
        'call_with_this_handle': extended_attribute_value_contains(call_with, 'ThisValue'),
        'custom': is_custom,
        'name': operation.name,
        'return_cpp_type': cpp_type(operation.idl_type, 'RefPtr'),
    }
    contents.update(generate_arguments_contents(operation.arguments, call_with_this_handle))
    return contents


def generate_arguments_contents(arguments, call_with_this_handle):
    def argument_declaration(argument):
        return '%s %s' % (cpp_type(argument.idl_type), argument.name)

    def generate_argument(argument):
        return {
            'name': argument.name,
            'cpp_to_v8_conversion': cpp_to_v8_conversion(argument.idl_type, argument.name),
        }

    argument_declarations = [argument_declaration(argument) for argument in arguments]
    if call_with_this_handle:
        argument_declarations.insert(0, 'ScriptValue thisValue')
    return  {
        'argument_declarations': argument_declarations,
        'arguments': [generate_argument(argument) for argument in arguments],
        'handles': ['%sHandle' % argument.name for argument in arguments],
    }
