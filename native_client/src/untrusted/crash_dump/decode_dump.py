#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utility to decode a crash dump generated by untrusted_crash_dump.[ch]

Currently this produces a simple stack trace.
"""

import json
import optparse
import os
import posixpath
import subprocess
import sys


class CoreDecoder(object):
  """Class to process core dumps."""

  def __init__(self, main_nexe, nmf_filename,
               addr2line, library_paths, platform):
    """Construct and object to process core dumps.

    Args:
      main_nexe: nexe to resolve NaClMain references from.
      nmf_filename: nmf to resovle references from.
      addr2line: path to appropriate addr2line.
      library_paths: list of paths to search for libraries.
      platform: platform string to use in nmf files.
    """
    self.main_nexe = main_nexe
    self.nmf_filename = nmf_filename
    if nmf_filename == '-':
      self.nmf_data = {}
    else:
      self.nmf_data = json.load(open(nmf_filename))
    self.addr2line = addr2line
    self.library_paths = library_paths
    self.platform = platform

  def _SelectModulePath(self, filename):
    """Select which path to get a module from.

    Args:
      filename: filename of a module (as appears in phdrs).
    Returns:
      Full local path to the file.
      Derived by consulting the manifest.
    """
    # For some names try the main nexe.
    # NaClMain is the argv[0] setup in sel_main.c
    # (null) shows up in chrome.
    if self.main_nexe is not None and filename in ['NaClMain', '', '(null)']:
      return self.main_nexe
    filepart = posixpath.basename(filename)
    nmf_entry = self.nmf_data.get('files', {}).get(filepart, {})
    nmf_url = nmf_entry.get(self.platform, {}).get('url')
    # Try filename directly if not in manifest.
    if nmf_url is None:
      return filename
    # Look for the module relative to the manifest (if any),
    # then in other search paths.
    paths = []
    if self.nmf_filename != '-':
      paths.append(os.path.dirname(self.nmf_filename))
    paths.extend(self.library_paths)
    for path in paths:
      pfilename = os.path.join(path, nmf_url)
      if os.path.exists(pfilename):
        return pfilename
    # If nothing else, try the path directly.
    return filename

  def _DecodeAddressSegment(self, segments, address):
    """Convert an address to a segment relative one, plus filename.

    Args:
      segments: a list of phdr segments.
      address: a process wide code address.
    Returns:
      A tuple of filename and segment relative address.
    """
    for segment in segments:
      for phdr in segment['dlpi_phdr']:
        start = segment['dlpi_addr'] + phdr['p_vaddr']
        end = start + phdr['p_memsz']
        if address >= start and address < end:
          return (segment['dlpi_name'], address - segment['dlpi_addr'])
    return ('(null)', address)

  def _Addr2Line(self, segments, address):
    """Use addr2line to decode a code address.

    Args:
      segments: A list of phdr segments.
      address: a code address.
    Returns:
      A list of dicts containing: function, filename, lineno.
    """
    filename, address = self._DecodeAddressSegment(segments, address)
    filename = self._SelectModulePath(filename)
    if not os.path.exists(filename):
      return [{
          'function': 'Unknown_function',
          'filename': 'unknown_file',
          'lineno': -1,
      }]
    # Use address - 1 to get the call site instead of the line after.
    address -= 1
    cmd = [
        self.addr2line, '-f', '--inlines', '-e', filename, '0x%08x' % address,
    ]
    process = subprocess.Popen(cmd, stdout=subprocess.PIPE)
    process_stdout, _ = process.communicate()
    assert process.returncode == 0
    lines = process_stdout.splitlines()
    assert len(lines) % 2 == 0
    results = []
    for index in xrange(len(lines) / 2):
      func = lines[index * 2]
      afilename, lineno = lines[index * 2 + 1].split(':', 1)
      results.append({
          'function': func,
          'filename': afilename,
          'lineno': int(lineno),
      })
    return results

  def LoadAndDecode(self, core_path):
    """Given a core.json file, load and embellish with decoded addresses.

    Args:
      core_path: source file containing a dump.
    Returns:
      An embelished core dump dict (decoded code addresses).
    """
    core = json.load(open(core_path))
    for frame in core['frames']:
      frame['scopes'] = self._Addr2Line(core['segments'], frame['prog_ctr'])
    return core

  def StackTrace(self, info):
    """Convert a decoded core.json dump to a simple stack trace.

    Args:
      info: core.json info with decoded code addresses.
    Returns:
      A list of dicts with filename, lineno, function (deepest first).
    """
    trace = []
    for frame in info['frames']:
      for scope in frame['scopes']:
        trace.append(scope)
    return trace

  def PrintTrace(self, trace, out):
    """Print a trace to a file like object.

    Args:
      trace: A list of [filename, lineno, function] (deepest first).
      out: file like object to output the trace to.
    """
    for scope in trace:
      out.write('%s at %s:%d\n' % (
          scope['function'],
          scope['filename'],
          scope['lineno']))


def Main(args):
  parser = optparse.OptionParser(
      usage='USAGE: %prog [options] <core.json>')
  parser.add_option('-m', '--main-nexe', dest='main_nexe',
                    help='nexe to resolve NaClMain references from')
  parser.add_option('-n', '--nmf', dest='nmf_filename', default='-',
                    help='nmf to resolve references from')
  parser.add_option('-a', '--addr2line', dest='addr2line',
                    help='path to appropriate addr2line')
  parser.add_option('-L', '--library-path', dest='library_paths',
                    action='append', default=[],
                    help='path to search for shared libraries')
  parser.add_option('-p', '--platform', dest='platform',
                    help='platform in a style match nmf files')
  options, args = parser.parse_args(args)
  if len(args) != 1:
    parser.print_help()
    sys.exit(1)
  decoder = CoreDecoder(
      main_nexe=options.main_nexe,
      nmf_filename=options.nmf_filename,
      addr2line=options.add2line,
      library_paths=options.library_paths,
      platform=options.platform)
  info = decoder.LoadAndDecode(args[0])
  trace = decoder.StackTrace(info)
  decoder.PrintTrace(trace, sys.stdout)


if __name__ == '__main__':
  Main(sys.argv[1:])
