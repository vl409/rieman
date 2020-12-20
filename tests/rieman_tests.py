
#
# Copyright (C) 2017-2019 Vladimir Homutov
#

# This file is part of Rieman.
#
# Rieman is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# Rieman is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License

import sys, subprocess, re, tempfile, signal, time, os, traceback

rie_tests = [

    # usage display with correct and wrong option
    { 'name': 'commandline',
        'show-help': {
            'cmd': 'build/rieman -h',
            'stdout': 'Usage: build/rieman \[-h\] \[-v\[v\]\] \[-c \<config\>\]',
            'stderr': '^$',
            'rc': 0,
            'timeout': 1 },

        'show-version': {
            'cmd': 'build/rieman -v',
            'stdout': 'rieman \d\.\d',
            'stderr': '^$',
            'rc': 0,
            'timeout': 1 },

        'show-verbose-version': {
            'cmd': 'build/rieman -vv',
            'stdout': 'Rieman \d\.\d\.\d Copyright \(c\) \d\d\d\d-\d\d\d\d .*prefix:.*debug:.*',
            'stderr': '^$',
            'rc': 0,
            'timeout': 1 },

        'wrong-arg': {
            'cmd': 'build/rieman -foo',
            'stdout': 'Usage: build/rieman \[-h\] \[-v\[v\]\] \[-c \<config\>\]',
            'stderr': 'Unknown argument "-foo"',
            'rc': 1,
            'timeout': 1 },

        'no-log-arg': {
            'cmd': 'build/rieman -l',
            'stdout': 'Usage: build/rieman \[-h\] \[-v\[v\]\] \[-c \<config\>\]',
            'stderr': 'option "-l" requires argument',
            'rc': 1,
            'timeout': 1 },

        'wrong-arg-and-log': {
            'cmd': 'build/rieman -l foo -foo',
            'stdout': 'Usage: build/rieman \[-h\] \[-v\[v\]\] \[-c \<config\>\]',
            'stderr': 'Unknown argument "-foo"',
            'rc': 1,
            'timeout': 1 },

        'bad-log': {
            'cmd': 'build/rieman -l /non-existing',
            'stdout': '.*',
            'stderr': 'failed to create log',
            'rc': 1,
            'timeout': 1 },

        'exec': [ 'show-help', 'show-version', 'show-verbose-version',
                  'wrong-arg', 'no-log-arg', 'wrong-arg-and-log', 'bad-log' ]
    },
    { 'name': 'invalid configuration',
        'noexist': {
            'cmd': 'build/rieman -c foo',
            'stdout': '^\[.*\] -log- using configuration file \'foo\'.*\[.*\] -err- open\(\"foo\"\) failed.*',
            'stderr': '',
            'rc': 1,
            'timeout': 1 },

        'missing-name': {
            'cmd': 'build/rieman -c',
            'stdout': 'Usage: build/rieman \[-h\] \[-v\[v\]\] \[-c \<config\>\]',
            'stderr': 'option "-c" requires argument',
            'rc': 1,
            'timeout': 1 },

        'bad-conf': {
            'cmd': 'build/rieman -c tests/conf/bad.conf',
            'stdout': 'syntax error.*configuration load failed',
            'stderr': '.*',
            'rc': 1,
            'timeout': 1 },

        'bad-key': {
            'cmd': 'build/rieman -c tests/conf/bad-key.conf',
            'stdout': 'conversion to bool failed: unknown value \'not-a-boolean\'.*configuration load failed',
            'stderr': '.*',
            'rc': 1,
            'timeout': 1 },

        'bad-key-validate': {
            'cmd': 'build/rieman -c tests/conf/bad-key-validate.conf',
            'stdout': 'conversion to bool failed.*configuration load failed',
            'stderr': '.*',
            'rc': 1,
            'timeout': 1 },

        'bad-key-2': {
            'cmd': 'build/rieman -c tests/conf/bad-key-2.conf',
            'stdout': 'conversion to uint32 failed: no digits.*configuration load failed',
            'stderr': '.*',
            'rc': 1,
            'timeout': 1 },

        'bad-key-3': {
            'cmd': 'build/rieman -c tests/conf/bad-key-3.conf',
            'stdout': 'conversion to uint32 failed: strtol.*configuration load failed',
            'stderr': '.*',
            'rc': 1,
            'timeout': 1 },

        'bad-key-4': {
            'cmd': 'build/rieman -c tests/conf/bad-key-4.conf',
            'stdout': 'invalid variant \'unknown\' of key \'layout\.corner\'.*configuration load failed',
            'stderr': '.*',
            'rc': 1,
            'timeout': 1 },

        'bad-key-5': {
            'cmd': 'build/rieman -c tests/conf/bad-key-5.conf',
            'stdout': 'invalid variant \'unknown-mask\' of key \'window\.layer\'.*configuration load failed',
            'stderr': '.*',
            'rc': 1,
            'timeout': 1 },

        'skin-bad-key': {
            'cmd': 'build/rieman -c tests/conf/skin-bad-key.conf',
            'stdout': 'conversion to hex failed.*Failed to load skin configuration',
            'stderr': 'rieman failed to start due to fatal error',
            'rc': 1,
            'timeout': 1 },

        'skin-bad-key-2': {
            'cmd': 'build/rieman -c tests/conf/skin-bad-key-2.conf',
            'stdout': 'conversion of \'NODBL\' to double failed: no digits.*Failed to load skin configuration',
            'stderr': 'rieman failed to start due to fatal error',
            'rc': 1,
            'timeout': 1 },

        'skin-bad-key-3': {
            'cmd': 'build/rieman -c tests/conf/skin-bad-key-3.conf',
            'stdout': 'conversion of .* to double failed: strtod.*Failed to load skin configuration',
            'stderr': 'rieman failed to start due to fatal error',
            'rc': 1,
            'timeout': 1 },

      'skin-bad-key-log': {
            'cmd': 'build/rieman -c tests/conf/skin-bad-key.conf -l build/test-skin-bad-key.log',
            'stdout': '.*',
            'stderr': 'rieman failed to start due to fatal error.*error log below:',
            'rc': 1,
            'timeout': 1 },

        'exec': [ 'noexist', 'missing-name', 'bad-conf',
                  'bad-key', 'bad-key-2', 'bad-key-3', 'bad-key-4', 'bad-key-5',
                  'bad-key-validate', 'skin-bad-key', 'skin-bad-key-2',
                  'skin-bad-key-3', 'skin-bad-key-log'
                ]
    },

    # normal execution with valid config
    { 'name': 'running',

        'default-exec': {
            'cmd': 'build/rieman',
            'stdout': '^\[.*\] -log- rieman ver\.\d\.\d\.\d .*started\.\.\.\n\[.*\] -log- using configuration file \'./conf/rieman.conf\'\n\[.*\] -log- desktop geometry is \d+ x \d+.*',
            'stderr': '.*',
            'rc': 0,
            'timeout': 1 },

        'xdg': {
            'cmd': 'build/rieman',
            'env': { 'XDG_CONFIG_HOME': 'foo', 'XDG_DATA_HOME': 'foo' },
            'stdout': '^\[.*\] -log- rieman ver\.\d\.\d\.\d .*started\.\.\.\n\[.*\] -log- using configuration file \'./conf/rieman.conf\'\n\[.*\] -log- desktop geometry is \d+ x \d+.*',
            'stderr': '.*',
            'rc': 0,
            'timeout': 1 },

        'c1': {
            'cmd': 'build/rieman -c tests/conf/c1.conf',
            'stdout': '^\[.*\] -log- rieman ver\.\d\.\d\.\d .*started\.\.\.\n\[.*\] -log- using configuration file \'tests/conf/c1.conf\'\n\[.*\] -log- desktop geometry is \d+ x \d+.*',
            'stderr': '.*',
            'rc': 0,
            'timeout': 1 },

        'c2': {
            'cmd': 'build/rieman -c tests/conf/c2.conf',
            'stdout': '^\[.*\] -log- rieman ver\.\d\.\d\.\d .*started\.\.\.\n\[.*\] -log- using configuration file \'tests/conf/c2.conf\'\n\[.*\] -log- desktop geometry is \d+ x \d+.*',
            'stderr': '.*',
            'rc': 0,
            'timeout': 1 },

        'c3': {
            'cmd': 'build/rieman -c tests/conf/c3.conf',
            'stdout': '^\[.*\] -log- rieman ver\.\d\.\d\.\d .*started\.\.\.\n\[.*\] -log- using configuration file \'tests/conf/c3.conf\'\n\[.*\] -log- desktop geometry is \d+ x \d+.*',
            'stderr': '.*',
            'rc': 0,
            'timeout': 1 },

        'exec': [ 'default-exec', 'xdg', 'c1', 'c2', 'c3' ]
    },

    # skin-related
    { 'name': 'skins',

        'skin-1': {
            'cmd': 'build/rieman -c tests/conf/skin-1.conf',
            'stdout': '^\[.*\] -log- rieman ver\.\d\.\d\.\d .*started\.\.\.\n\[.*\] -log- using configuration file \'tests/conf/skin-1.conf\'\n\[.*\] -log- desktop geometry is \d+ x \d+.*',
            'stderr': '.*',
            'rc': 0,
            'timeout': 1 },

        'skin-3': {
            'cmd': 'build/rieman -c tests/conf/skin-3.conf',
            'stdout': '^\[.*\] -log- rieman ver\.\d\.\d\.\d .*started\.\.\.\n\[.*\] -log- using configuration file \'tests/conf/skin-3.conf\'.*\[.*\] -err- cairo_image_surface_create_from_png\(\'.*missing.png\'\).*',
            'stderr': 'rieman failed to start',
            'rc': 0,
            'timeout': 1 },

        'exec': [ 'skin-1', 'skin-3' ]
    },
    # signals
    { 'name': 'signals',

        'reload': {
            'cmd': 'build/rieman -c tests/conf/skin-1.conf',
            'stdout': '\*\*\* starting event loop.*\*\*\* reload signal received',
            'stderr': '.*',
            'rc': 0,
            'reload' : True,
            'timeout': 1 },

        'term-and-withdrawn': {
            'cmd': 'build/rieman -w -c tests/conf/skin-1.conf',
            'stdout': '\*\*\* starting event loop.*\*\*\* terminate signal received',
            'stderr': '.*',
            'rc': 0,
            'terminate' : True,
            'timeout': 1 },

        'term-and-log': {
            'cmd': 'build/rieman -c tests/conf/skin-1.conf-l build/test-term-and-log.log',
            'stdout': '.*',
            'stderr': '.*',
            'rc': 0,
            'terminate' : True,
            'timeout': 1 },

        'exec': [ 'reload', 'term-and-withdrawn', 'term-and-log' ]
    },
];

class Alarm(Exception):
    pass

def alarm_handler(signum, frame):
    raise Alarm

def tc_match(pattern, actual, name):

    rx = re.compile(pattern, re.DOTALL)

    if rx.search(actual.decode('utf-8')) == None:
       print("\n !!! actual %s does not match expected pattern" % name)
       print("\n actual %s:\n  >>>\n%s" % (name, actual.decode('utf-8')));
       print("  <<<\n  Expected regex: '%s'" % (pattern));
       return None

    return True

def tc_exec(tc):

    procs = {}
    tfiles = {}
    result = {}

    # omg, python requires this to be done, can't just assign x[a][b]
    for proc in tc['exec']:
        tfiles[proc] = {}
        result[proc] = {}

    # open temporary files for client/server stdout/stderr
    for proc in tc['exec']:
        for out in ['stdout', 'stderr']:
            tfiles[proc][out] = tempfile.TemporaryFile()

    # execute server and then client and redirect output to temp files
    max_tmout = 0
    for proc in tc['exec']:
        # Let the 1st (typically server) some time to initialize (open socket)
        time.sleep(0.2)
        tc_env = os.environ.copy()

        if tc[proc].get('env'):
            tc_env.update(tc[proc]['env'])

        procs[proc] = subprocess.Popen(tc[proc]['cmd'].split(' '),
                                       stdout=tfiles[proc]['stdout'],
                                       stderr=tfiles[proc]['stderr'],
                                       universal_newlines=True,
                                       env = tc_env)

        if tc[proc]['timeout'] > max_tmout:
            max_tmout = tc[proc]['timeout']

    try:

        signal.signal(signal.SIGALRM, alarm_handler)
        signal.alarm(max_tmout)

        for proc in tc['exec']:
            if tc[proc].get('reload') == True:
                time.sleep(0.2)
                os.kill(procs[proc].pid, signal.SIGUSR1)

            if tc[proc].get('terminate') == True:
                time.sleep(0.2)
                os.kill(procs[proc].pid, signal.SIGTERM)

            procs[proc].wait()
            result[proc]['rc'] = procs[proc].returncode

        signal.alarm(0)

    except Alarm:
        for proc in tc['exec']:
            if os.path.exists("/proc/%d" % procs[proc].pid):
                # TODO: kill does not allow coverage to be written
                #       only kill if terminate failed;
                #procs[proc].kill()
                procs[proc].terminate()
                procs[proc].wait()
                result[proc]['rc'] = procs[proc].returncode

    for proc in tc['exec']:
        for out in ['stdout', 'stderr']:
            tfiles[proc][out].flush()
            tfiles[proc][out].seek(0, 0)
            result[proc][out] = tfiles[proc][out].read()

    return result

def tc_announce(tc):

    tcname = tc['name']

    width = INDENT - len(tcname)
    spaces = " " * width

    sys.stdout.write(">> %s%s" % (tcname, spaces))

    if verbose == None:
        return

    sys.stdout.write("=== '%s' ===\n" % tc['name'])
    for proc in tc['exec']:
        sys.stdout.write("  %s: %s\n" % (proc, tc[proc]['cmd']))


def tc_result(tc, status, info = ""):

    if status == True:
        sys.stdout.write("pass\n")
    elif status == False:
        sys.stdout.write("fail\n")
    else:
        sys.stdout.write("exception while running test: %s\n" % info)

def run_test(testcase):

    result = tc_exec(testcase)

    passed = True

    for proc in testcase['exec']:
        for out in ['stdout', 'stderr']:
            if verbose:
                print("|| %s: >>> %s <<<\n" % (proc + ' ' + out, result[proc][out]))

            if tc_match(testcase[proc][out], result[proc][out], proc + ' ' + out) == None:
                passed = False

        if testcase[proc].get('rc') and (testcase[proc]['rc'] != result[proc]['rc']):
            print("\n !!! actual rc %d does not match expected %d for %s" %
                (result[proc]['rc'], testcase[proc]['rc'], proc))
            passed = False

    return passed

verbose = os.getenv('RIE_TEST_VERBOSE')

if len(sys.argv) > 1:
    test_filter = sys.argv[1]
else:
    test_filter = ""

failed = 0
max_w = 0

for tc in rie_tests:
    if len(tc['name']) > max_w:
        max_w = len(tc['name'])

INDENT = max_w + 4

for tc in rie_tests:

    tc_announce(tc)

    tm = re.compile(test_filter, re.IGNORECASE)
    if tm.search(tc['name']) == None:
        sys.stdout.write('skipped\n')
        continue

    try:
        if run_test(tc) != True:
            failed = failed + 1
            tc_result(tc, False)
        else:
            tc_result(tc, True)
    except:
        ex, value, trace = sys.exc_info()
        tc_result(tc, None, str(value))
        traceback.print_tb(trace)

if failed > 0:
    sys.exit(1)
