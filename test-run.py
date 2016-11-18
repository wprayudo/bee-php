#!/usr/bin/env python

import os
import sys
import shlex
import shutil
import subprocess

import time

from lib.bee_server import BeeServer

from pprint import pprint

def read_popen(cmd):
    path = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE)
    path.wait()
    return path.stdout.read()

def read_popen_config(cmd):
    cmd = os.environ.get('PHP_CONFIG', 'php-config') + ' ' + cmd
    return read_popen(cmd)

def find_php_bin():
    path = read_popen('which php').strip()
    if (path.find('phpenv') != -1):
        version = read_popen('phpenv global')
        if (version.find('system') != -1):
            return '/usr/bin/php'
        return '~/.phpenv/versions/{0}/bin/php'.format(version.strip())
    return path

def prepare_env(php_ini):
    if os.path.isdir('var'):
        shutil.rmtree('var')
    if not os.path.isdir('var') and not os.path.exists('var'):
        os.mkdir('var')
    shutil.copy('test/shared/phpunit.xml', 'var')
    test_dir_path = os.path.abspath(os.path.join(os.getcwd(), 'test'))
    test_lib_path = os.path.join(test_dir_path, 'phpunit.phar')
#    shutil.copy('test/shared/bee.ini', 'var')
    shutil.copy(php_ini, 'var')
    shutil.copy('modules/bee.so', 'var')

def main():
    path = os.path.dirname(sys.argv[0])
    if not path:
        path = '.'
    os.chdir(path)
    if '--prepare' in sys.argv:
        prepare_env('test/shared/bee.ini')
        exit(0)
    srv = None
    srv = BeeServer()
    srv.script = 'test/shared/db.lua'
    srv.start()
    test_dir_path = os.path.abspath(os.path.join(os.getcwd(), 'test'))
    test_cwd = os.path.dirname(srv.vardir)
    test_lib_path = ""
    try:
        shutil.copy('test/shared/phpunit.xml', os.path.join(test_cwd, 'phpunit.xml'))
        shutil.copy('modules/bee.so', test_cwd)
        test_lib_path = os.path.join(test_dir_path, 'phpunit.phar')

        version = read_popen_config('--version').strip(' \n\t') + '.'
        version1 = read_popen_config('--extension-dir').strip(' \n\t')
        version += '\n' + ('With' if version1.find('non-zts') == -1 else 'Without') + ' ZTS'
        version += '\n' + ('With' if version1.find('no-debug') == -1 else 'Without') + ' Debug'
        print('Running against ' + version)

        for php_ini in [
            'test/shared/bee-1.ini',
            'test/shared/bee-2.ini',
            'test/shared/bee-3.ini'
        ]:
            cmd = ''
            shutil.copy(php_ini, os.path.join(test_cwd, 'bee.ini'))
            if '--flags' in sys.argv:
                os.environ['ZEND_DONT_UNLOAD_MODULES'] = '1'
                os.environ['USE_ZEND_ALLOC'] = '0'
                os.environ['MALLOC_CHECK_'] = '1'
            if '--valgrind' in sys.argv:
                cmd = cmd + 'valgrind --leak-check=full --log-file='
                cmd = cmd + os.path.basename(php_ini).split('.')[0] + '.out '
                cmd = cmd + '--suppressions=test/shared/valgrind.sup '
                cmd = cmd + '--keep-stacktraces=alloc-and-free'
                cmd = cmd + ' --freelist-vol=2000000000 '
                cmd = cmd + '--malloc-fill=0 --free-fill=0 '
                cmd = cmd + '--num-callers=50 ' + find_php_bin()
                cmd = cmd + ' -c bee.ini {0}'.format(test_lib_path)
            elif '--gdb' in sys.argv:
                cmd = cmd + 'gdb {0} --ex '.format(find_php_bin())
                cmd = cmd + '"set args -c bee.ini {0}"'.format(test_lib_path)
            elif '--lldb' in sys.argv:
                cmd = cmd + 'lldb -- {0} -c bee.ini {1}'.format(find_php_bin(), test_lib_path)
            elif '--strace' in sys.argv:
                cmd = cmd + 'strace ' + find_php_bin()
                cmd = cmd + ' -c bee.ini {0}'.format(test_lib_path)
            else:
                print find_php_bin()
                cmd = '{0} -c bee.ini {1}'.format(find_php_bin(), test_lib_path)
                print cmd

            print('Running "%s" with "%s"' % (cmd, php_ini))
            proc = subprocess.Popen(cmd, shell=True, cwd=test_cwd)
            rv = proc.wait()
            if rv != 0:
                return -1

    finally:
        a = [
                os.path.join(test_cwd, 'bee.ini'),
                os.path.join(test_cwd, 'phpunit.xml'),
                os.path.join(test_cwd, 'bee.so'),
        ]
        for elem in a:
            if os.path.exists(elem):
                os.remove(elem)

    return 0

exit(main())
