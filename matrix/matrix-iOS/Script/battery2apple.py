#!/usr/bin/env python3
#-*- coding: utf-8 -*-
'''
@Author: mattzheng
@since: 2019-06-06 16:11:49
@lastTime: 2019-06-06 16:12:16
@LastAuthor: Do not edit
'''

import json
import os
import sys
import cgi
import html
from datetime import datetime
import traceback
import importlib

MM_IMG_LIST = ['MicroMessenger', 'WeChat', 'WeChatShareExtension', 'WeChatWatchExtension']
Component_IMG_LIST = ['MMCommon', 'MMCoreFoundation', 'MMPLCrashReporter', 'PublicComponentDylib', 'MMFoundationDylib','MMLibHooks', 'matrixreport', 'Matrix', 'QBar']

importlib.reload(sys)
# sys.setdefaultencoding('utf8')

def is_image_wechat(image_name):
    return image_name in MM_IMG_LIST + Component_IMG_LIST

def dump_json(obj):
    return json.dumps(obj, indent=4,
            sort_keys=True, separators=(',',': '))

def get_system_info(report):
    '''Get system information from report'''
    return report.get('system', None)

def get_report_info(report):
    '''Get report information from report'''
    return report.get('report', None)

def get_crash_info(report):
    '''Get crash information from report'''
    return report.get('crash', None)

def get_cpu_type(arch):
    return {'arm64': 'ARM-64',
            'arm': 'ARM',
            'x86': 'X86',
            'x86_64': 'X86_64'}.get(arch, 'Unknow')

def get_cpu_arch(report):
    system = get_system_info(report)
    arch = system.get('cpu_arch', '')
    return get_cpu_type(arch)

def get_binary_img_info(report):
    # key: name, value: uuid
    img_info = {}

    images = report.get('binary_images', [])
    for image in images:
        name = os.path.basename(image.get("name"))
        if name not in img_info:
            img_info[str(name)] = image

    return img_info 


def get_belong_img(report, addr):
    images = report.get('binary_images', [])
    for img in images:
        if img['image_addr'] <= addr <= (img['image_addr'] + img['image_size']):
            return img

    return None
    
CPU_TYPE_ARM = 12
CPU_ARCH_ABI64 = 0x01000000
CPU_TYPE_ARM64 = CPU_TYPE_ARM | CPU_ARCH_ABI64
CPU_TYPE_X86 = 7
CPU_TYPE_X86_64 = CPU_TYPE_X86 | CPU_ARCH_ABI64

CPU_SUBTYPE_ARM_V6 = 6
CPU_SUBTYPE_ARM_V7 = 9
CPU_SUBTYPE_ARM_V7F = 10
CPU_SUBTYPE_ARM_V7S = 11
CPU_SUBTYPE_ARM_V7K = 12
CPU_SUBTYPE_ARM_V6M = 14
CPU_SUBTYPE_ARM_V7M = 15
CPU_SUBTYPE_ARM_V7EM = 16
CPU_SUBTYPE_ARM_V8 = 13

CPU_ARM_TYPES = {
    CPU_SUBTYPE_ARM_V6: 'armv6',
    CPU_SUBTYPE_ARM_V7: 'armv7',
    CPU_SUBTYPE_ARM_V7F: 'armv7f',
    CPU_SUBTYPE_ARM_V7S: 'armv7s',
    CPU_SUBTYPE_ARM_V7K: 'armv7k',
    CPU_SUBTYPE_ARM_V6M: 'armv6m',
    CPU_SUBTYPE_ARM_V7M: 'armv7m',
    CPU_SUBTYPE_ARM_V7EM: 'armv7em',
    CPU_SUBTYPE_ARM_V8: 'armv8',
}

def get_detail_cpu_arch(major, minor):
    if major == CPU_TYPE_ARM:
        cpu_type = CPU_ARM_TYPES.get(minor, 'arm')
    elif major == CPU_TYPE_ARM64:
        return 'arm64'
    elif major == CPU_TYPE_X86:
        return 'i386'
    elif major == CPU_TYPE_X86_64:
        return 'x86_64'

    return 'unknown({0},{1})'.format(major, minor)

def get_last_exception(report):
    report_info = get_report_info(report)
    process = report_info.get('process', None)
    if not process:
        return None
    return process.get('last_dealloced_nsexception', None)

def get_time(report):
    '''Get crash time from report
    @return time like "2015-12-28 17:48:03 +0800"
    '''

    info = get_report_info(report)
    timestamp = info.get('timestamp', None)
    if not timestamp:
        return ''

    if type(timestamp) == type(0):
        return datetime.fromtimestamp(timestamp).strftime("%Y-%m-%d %H:%M:%S")
    else:
        return timestamp

        #from dateutil import parser, tz
        #date = parser.parse(timestamp)
        #return date.astimezone(tz.tzlocal())\
        #    .strftime('%Y-%m-%d %H:%M:%S.000 %z')

def get_crash_thread(report):
    crash = get_crash_info(report)
    if not crash:
        return [] 
    print(crash)
    threads = crash['threads']
    for thread in threads:
        crashed = thread.get('crashed', False)
        if crashed:
            return thread

    return crash.get('crashed_thread', None)


def parse_user_info(report):
    user_info = report.get("user", None)
    if not user_info:
        return []

    result = ['\nUser Info: {']
    if user_info and user_info.get("WeChat"):
        uin = user_info["WeChat"].get("uin", None)
        if uin:
            result.append(show_color('    Uin:             {0}'.format(uin)))
        if user_info["WeChat"].get("UsrName"):
            result.append('    UsrName:         {0}'.format(user_info["WeChat"].get("UsrName")))
        if user_info["WeChat"].get("heavyUser", -1) != -1:
            result.append('    heavyUser:       {0}'.format(user_info["WeChat"].get("heavyUser")))
        if user_info["WeChat"].get("heavyUserType", -1) != -1:
            result.append('    heavyUserType:   {0}'.format(user_info["WeChat"].get("heavyUserType")))
        if user_info["WeChat"].get("heavyPoint", -1) != -1:
            result.append('    heavyPoint:      {0}'.format(user_info["WeChat"].get("heavyPoint")))
        if user_info["WeChat"].get("DumpType", -1) != -1:
            result.append('    dumpType:        {0}'.format(user_info["WeChat"].get("DumpType")))
        if user_info["WeChat"].get("blockTime"):
            result.append('    blockTime:       {0}'.format(user_info["WeChat"].get("blockTime")))
        if user_info["WeChat"].get("LastScene"):
            result.append('    LastScene:       {0}'.format(user_info["WeChat"].get("LastScene")))
        if user_info["WeChat"].get("SecondLastScene"):
            result.append('    SecondLastScene: {0}'.format(user_info["WeChat"].get("SecondLastScene")))
        if user_info["WeChat"].get("WeAppScene"):
            result.append('    WeAppScene:      {0}'.format(user_info["WeChat"].get("WeAppScene")))
        if user_info["WeChat"].get("ExistWeAppCount"):
            result.append('    ExistWeAppCount: {0}'.format(user_info["WeChat"].get("ExistWeAppCount")))
        if user_info["WeChat"].get("device_uuid"):
            result.append('    device_uuid:     {0}'.format(user_info["WeChat"].get("device_uuid")))
    if len(result) < 2:
        result.append(json.dumps(user_info, indent=4))
    result.append('}')
    return result

def parse_system_info(report):
    '''Parse system information from report.
    @return header lines
    '''

    sys = get_system_info(report)
    if not sys:
        return []
    info = get_report_info(report)

    _s = lambda x: sys.get(x, '')
    _i = lambda x: info.get(x, '')

    headers = ['System Info: {']
    device = _s('machine')
    headers.append('    Device:      {0}'.format(device))
    headers.append('    CpuArch:     {0}'.format(_s('cpu_arch')))
    headers.append('    OS Version:  {0} {1} ({2})'.format(_s('system_name'), _s('system_version'), _s('os_version')))
    global os_table
    os_table ='%s_%s' %(_s('system_version').replace('.','_'),  _s('os_version'))
    if _s('cpu_arch') == "arm64e":
        os_table += '_%s' % _s('cpu_arch')
    print("os_table:", os_table)
    
    user_info = report.get("user", None)
    if user_info and user_info.get('WeChat'):
        jb_info = user_info.get('WeChat').get('Jailbreak', None) 
        if jb_info:
            headers.append('    Jailbreak:   {0}'.format(jb_info))

    mem = sys.get("memory")
    if mem:
        fmt = lambda x: "    Mem {0:6}:  {1:4} M".format(x, int(mem[x])/1024/1024)
        headers.append(fmt("usable")) 
        headers.append(show_color(fmt("free"))) 
        headers.append(fmt("size")) 

    headers.append('}')
    return headers
def show_color(str,color='blue'):
    return '<em style="color:%s;">%s</em>'%(color,str)

def zombie_exception(report):
    crash = get_crash_info(report)
    error = crash['error']
    mach = error['mach']
    exc_name = mach.get('exception_name', '0')
    code_name = mach.get('code_name', '0x00000000')

    if exc_name != 'EXC_BAD_ACCESS' or code_name != 'KERN_INVALID_ADDRESS':
        return False

    last_exception = get_last_exception(report)
    if not last_exception:
        return False

    last_addr = last_exception['address']
    thread = get_crash_thread(report)
    registers = thread['registers']['basic']
    for reg, addr in registers:
        if addr == last_addr:
            return True

    return False


def parse_error_info(report):
    crash = get_crash_info(report)
    if not crash:
        return [] 
    error = crash.get('error')
    if not error:
        print("waring: no error found in crash")
        return []
    mach = error['mach']
    signal = error['signal']

    exc_name = mach.get('exception_name', '0')
    sig_name = signal.get('name', None)
    if not sig_name:
        sig_name = signal.get('signal', '')

    code_name = mach.get('code_name', '0x00000000')
    addr = error.get('address', '0')

    crash_thread = 0
    thread = get_crash_thread(report)
    if thread:
        crash_thread = thread['index']

    result = ['']
    result.append('Exception Type:  {0} ({1})'.format(exc_name, sig_name))
    result.append('Exception Codes: {0} at {1:016x}'.format(code_name, int(addr)))
    result.append('Crashed Thread:  {0}'.format(crash_thread))

    diagnosis = crash.get('diagnosis', None)
    print("fuck haha", diagnosis)
    if diagnosis:
        result.append('\nCrashDoctor Diagnosis: {0}'.format(diagnosis))
        print(result)
    return result

def parse_crash_reason(report):
    result = ['']
    reason_fmt = ("Application Specific Information"
                  "*** Terminating app due to uncaught exception '{0}', "
                  "reason: '{1}'")
    fmt = lambda x, y: reason_fmt.format(x, y)

    crash = get_crash_info(report)
    if not crash:
        return [] 

    error = crash.get('error')
    if not error:
        print("warning: no error found in crash")
        return []
    crash_type = error['type']

    user_exception = error.get('user_reported', None)
    ns_exception = error.get('nsexception', None)
    if ns_exception:
        result.append(fmt(ns_exception['name'], error.get('reason', '')))
    elif zombie_exception(report):
        last_exception = get_last_exception(report)
        if (last_exception):
            result.append(fmt(last_exception['name'], last_exception['reason']))
    elif user_exception:
        result.append(fmt(user_exception['name'], error['reason']))
        line = user_exception.get('line_of_code', None)
        backtrace = user_exception.get('backtrace', [])
        if line or backtrace:
            result.append('Custom Backtrace:')
        if line:
            result.append('Line: {0}'.format(line))
        for entry in backtrace:
            result.append(entry)

    elif crash_type == 'cpp_exception':
        cpp_exception = error.get('cpp_exception', None)
        if cpp_exception and cpp_exception.get('name'):
            result.append(fmt(crash_type + ': '+ cpp_exception['name'], error.get('reason')))
    elif crash_type == 'deadlock':
        result.append('Application main thread deadlocked')
    return result

def parse_backtrace(result, img_info, stack_string, mask_pc=True):
    num = 0
    
    for trace in stack_string:
        try:
            if mask_pc:
                pc = trace['instruction_address'] & 0x0000000fffffffff
            else:
                pc = trace['instruction_address']
        except:
            traceback.print_exc()
            continue
        
        img_name = os.path.basename(trace['image_name'])
        img = img_info.get(img_name)
        if not img:
            print("fuck")
            for name in img_info:
                if img_info[name]['image_addr'] <= pc <= img_info[name]['image_addr']+ img_info[name]['image_size']:
                    img = img_info[name] 
                    break
        if not img:
            print("error, no img found for pc: %s, instruction_address: %s" % (pc, trace['instruction_address']))
            print(img_name)
            result.append('{0:<4}{1:31} 0x{2:016x}'.format(num, 'unknown', pc))
            num += 1
            continue

        uuid = img['uuid'].lower().replace('-', '') 
        obj_addr = img['image_addr']
        offset = pc - obj_addr 
        
        symbol_name = trace.get('object_name', None)
        symbol_addr = trace.get('object_address', 0)

        local_symbol_name = trace.get('object_name')
        #preamble = '{0:<4}{1} 0x{2:016x}'.format(trace.get("sample"), img_name, pc)
        #unsymbolicated = '0x{0:04x} + {1}'.format(obj_addr, pc-obj_addr)
        symbolicated = '(null)'
        is_unsymbolicated = False
        if symbol_name:
            symbol_name = symbol_name.replace('(in ', '(')[:-1]
            symbolicated = '{0} {1}'.format(trace.get("sample"), symbol_name)
            #symbolicated = '{0} + {1}'.format(symbol_name, pc-symbol_addr)
        else:
            is_unsymbolicated = True
        if symbol_name == '<redacted>':
            is_unsymbolicated = True

        if img_name in symbol_name:
            preamble = '+ {0}) [0x{1:016x}]'.format(pc-obj_addr, pc)
        else:
            preamble = '({0} + {1}) [0x{2:016x}]'.format(img_name, pc-obj_addr, pc)
        if is_unsymbolicated:
            line = '{0}'.format(preamble)
        else:
            line = '{0} {1}'.format(symbolicated, preamble)#, unsymbolicated)

        _frame = {"symbol": line, "child": []}     
        result.append(_frame)     
        if trace.get("child"):
            parse_backtrace(_frame['child'], img_info, trace['child'], mask_pc)
         
            
def format_stack(result, prefix, stack):
    for item in stack:
        result.append('{}  {}'.format(prefix, item['symbol']))
        if item.get('child'):
            format_stack(result, prefix+'  ', item['child']) 
        

def parse_stack_info(report):
    result = []
    stack_info = report.get('stack_string', [])
    if not stack_info:
        return 'empty stack_string'

    sys = get_system_info(report)
    _s = lambda x: sys.get(x, '')
    system_name = _s('system_name')
    mask_pc = True
    if system_name == 'macOS':
        mask_pc = False
        
    app = report["report"]['process_name']
    img_info = get_binary_img_info(report) 
    parse_backtrace(result, img_info, stack_info, mask_pc)
    
    text_stack = ['stack_info:'] 
    format_stack(text_stack, '  ', result) 
 
    return text_stack 

def parse_thread_list(report):
    crash = get_crash_info(report)
    if not crash:
        return [] 
    print(dump_json( crash))
    threads = crash['threads']

    result = []
    for thread in threads:
        result.append('')
        result += parse_thread_info(thread, report)

    return result

def get_register_order(cpu):
    cpu = cpu.lower()
    print("cpu %s" % cpu)
    arm = [ 'x'+str(i) for i in range(30)] + ['fp','sp', 'lr', 'pc',
           'cpsr']
    x86 = ['eax', 'ebx', 'ecx', 'edx', 'edi', 'esi', 'ebp',
           'esp', 'ss', 'eflags', 'eip', 'cs', 'ds', 'es',
           'fs', 'gs']
    x86_64 = ['rax', 'rbx', 'rcx', 'rdx', 'rdi', 'rsi', 'rbp',
              'rsp', 'r8', 'r9', 'r10', 'r11', 'r12', 'r13',
              'r14', 'r15', 'rip', 'rflags', 'cs', 'fs', 'gs']
    if cpu.startswith('arm'):
        return arm
    elif cpu in ('x86', 'i386', 'i486', 'i686'):
        return x86
    elif cpu == 'x86_64':
        return x86_64
    else: 
        return arm 

def parse_cpu_state(report):
    result = ['']
    crashed = get_crash_thread(report)
    if not crashed:
        return result
    index = crashed['index']

    system = get_system_info(report)
    cpu = get_detail_cpu_arch(system['binary_cpu_type'],
            system['binary_cpu_subtype'])
    cpu = get_cpu_type(cpu)
    result.append('Thread {0} crashed with {1} Thread State:'.format(index, cpu))

    registers = crashed.get('registers', {}).get('basic', {})
    order = get_register_order(cpu)
    if not order:
        return result

    line = ''
    for i, reg in enumerate(order):
        if (i != 0) and (i % 4 == 0):
            result.append(line[:-1])
            line = ''
        try:
            line += '{0:>6}: 0x{1:016x} '.format(reg, registers.get(reg, 0))
        except:
            continue

    if line:
        result.append(line[:-1])

    return result

def parse_binary_images(report):
    result = ['\nBinary Images:']
    system = get_system_info(report)
    if not system:
        return result

    exe_path = system['CFBundleExecutablePath']

    images = report.get('binary_images', [])
    images = sorted(images, key=lambda k: k['image_addr'])

    image_count = 0
    for image in images:
        try:
            image_count = image_count + 1
            cpu = image.get('cpu_type')
            if not cpu:
                cput = image.get('cpuType')
            #cpu_sub = image['cpu_subtype']
            addr = image['image_addr']
            size = image['image_size']
            path = image['name']
            name = os.path.basename(path)
            uuid = image['uuid'].lower().replace('-', '')
            print('parse_binary_images')
            print(name, uuid)
            is_base = '+' if path==exe_path else ' '
            if False:#image_count <=10 and not is_image_wechat(name) and simple_symbol_find.uuid_find(uuid) == False:
                #name =  "<em style=\"color: red;\">%s</em>"%name
                result.append('<em style="color: red;">{0:>#18x} - {1:>#18x} {2}{3}  <{4}> {5}</em>'\
                    .format(addr, addr+size-1, is_base, name,
                            uuid, path))
            else:
                if name == 'WeChat':
                    print('{0:>#18x} - {1:>#18x} {2}{3}  <{4}> {5}'.format(addr, addr+size-1, is_base, name,uuid, path))
                    pass
                result.append('{0:>#18x} - {1:>#18x} {2}{3}  <{4}> {5}'\
                    .format(addr, addr+size-1, is_base, name,
                            uuid, path))
        except:
            traceback.print_exc()
            print(image)
            return
            continue

    return result

def parse_mem_info(report):
    result = ['\nMemory: {']
    system = get_system_info(report)
    mem = system.get("memory")

    fmt = lambda x: "    {0:6}: {1:4} M,".format(x, int(mem[x])/1024/1024)
    result.append(fmt("usable")) 
    result.append(fmt("free")) 
    result.append(fmt("size")[:-1]) 
    result.append('}')
    
    return result 

def parse_app_info(report):
    result = ['\nApplication Info:{']
    system = get_system_info(report)
    if not system:
        return []

    _s = lambda x: system.get(x, '')
    info = get_report_info(report)
    _i = lambda x: info.get(x, '')

    app_stats = system.get('application_stats', None)
    if not app_stats:
        return []
    try:
        if app_stats.get('app_launch_time'):
            app_stats['app_launch_time'] = datetime.fromtimestamp(app_stats.get('app_launch_time')).strftime("%Y-%m-%d %H:%M:%S")
        app_stats['app_crash__time'] = get_time(report)
        
        app_stats['Process'] =  '{0} [{1}]'.format(_s('process_name'), _s('process_id'))
        app_stats["Identifier"] = '{0}'.format(_i('id'))
        app_stats['Version'] = '{0} ({1})'.format(_s('CFBundleShortVersionString'), _s('CFBundleVersion'))

    except:
        traceback.print_exc()

    user_info = report.get("user", None)
    if user_info and user_info.get("WeChat"):
        commit_id = user_info["WeChat"].get("commit_id", None)
        if commit_id:
            app_stats['Commit ID'] = '{0}'.format(commit_id)
        wcdb_commit_id = user_info["WeChat"].get("wcdb_commit_id", None)
        if wcdb_commit_id:
            app_stats['WCDB_Commit ID'] = '{0}'.format(wcdb_commit_id)
    app_stats['Code Type'] = '{0}'.format(get_cpu_arch(report))

    reason = "".join(parse_crash_reason(report))
    if reason:
        asi = 'Application Specific Information'
        app_stats[asi] = reason.replace(asi, '') 

    for item in parse_error_info(report):
        if not item:
            continue
        items = item.split(':')
        if len(items) == 2:
            app_stats[items[0].strip()] = items[1].strip()

    if app_stats:
        #result.append('\nApplication Info:\n{0}'\
        #    .format(dump_json(app_stats).replace('"', '')))

        for key in sorted(app_stats):
            fmt = lambda x: "    {0:36}:  {1}".format(key, app_stats[key])
            if key in ['Crashed Thread','Application Specific Information','Exception Type','Version','app_crash__time','app_launch_time']:
                result.append(show_color(fmt(key)))
            else:
                result.append(fmt(key))
            
        result.append('}\n')
    return result


def parse_extra_info(report):
    result = ['', 'Extra Information:']
    system = get_system_info(report)
    crash = get_crash_info(report)
    if not crash:
        return []
    error = crash.get('error')
    if not error:
        return []
    ns_exception = error.get('nsexception', None)
    if ns_exception:
        ref_obj = ns_exception.get('referenced_object', None)
        if ref_obj:
            result.append('Object referenced by NSException:')
            result.append(dump_json(ref_obj))

    crashed = get_crash_thread(report)
    if crashed:
        stack = crashed.get('stack', None)
        if stack:
            result.append('Stack Dump (0x{0:08x}-0x{1:08x}):'\
                    .format(stack['dump_start'], stack['dump_end']))
            result.append('')
            result.append(stack.get('contents', ''))
        notable_addr = crashed.get('notable_addresses', None)
        if notable_addr:
            for _tmp_stack in notable_addr:
                if  notable_addr[_tmp_stack].get('address', None):
                    notable_addr[_tmp_stack]['address'] = hex(notable_addr[_tmp_stack]['address'])
            result.append('Notable Addresses:\n{0}'\
                    .format(dump_json(notable_addr)))
    last_exception = get_last_exception(report)
    if last_exception:
        addr = last_exception['address']
        name = last_exception['name']
        reason = last_exception['reason']

        result.append('\nLast deallocated NSException (0x{0:016x}): {1}: {2}'\
                .format(addr, name, reason))

        ref_obj = last_exception.get('referenced_object', None)
        if ref_obj:
            result.append('Referenced object:\n{0}'\
                    .format(dump_json(ref_obj)))
        info = get_report_info(report)
        app = info['process_name']
        backtrace = parse_backtrace(crashed['backtrace'], app)
        result += backtrace

    '''
    app_stats = system.get('application_stats', None)
    if app_stats:
        result.append('\nApplication Stats:\n{0}'\
            .format(dump_json(app_stats)))
    '''

    return result

def parse_log_info(report):
    result = ['', 'MMLog:']
    user_info = report.get("user", None)
    if not user_info or not user_info.get("WeChat", None) or not user_info["WeChat"].get("log"):
        result.append("no log found...")
        return result 

    log_list = user_info["WeChat"]["log"]
    result += log_list

    return result

def parse_click_info(report):
    result = ['', 'Click:']
    user_info = report.get("user", None)
    if not user_info or not user_info.get("WeChat", None) or not user_info["WeChat"].get("click"):
        result.append("no click found...")
        return [] 

    click_list = user_info["WeChat"]["click"]
    result += click_list

    return result

def parse_other_info(report):
    result = ['']
    user_info = report.get("user", None)
    if not user_info or not user_info.get("WeChat", None) or not user_info["WeChat"].get("log"):
        print("no log found...")
        return result 

    log_list = user_info["WeChat"]["log"][-2:]
    result += log_list

    return result

def ks_json_2_apple(report, fout):
    headers = parse_system_info(report)
    for line in headers:
        fout.write(line+'\n')

    try:
        user_info = parse_user_info(report)
        for line in user_info:
            fout.write(line+'\n')
    except:
        traceback.print_exc() 
    
    app_info = parse_app_info(report)
    for line in app_info:
        fout.write(line+'\n')
    fout.write('\n\n')

    #other_info = parse_other_info(report)
    #for line in other_info:
    #    fout.write(line+'\n')
    stack_info = parse_stack_info(report)
    for line in stack_info:
        line = html.escape(line)
        fout.write(line+'\n')

    cpu_state = parse_cpu_state(report)
    for line in cpu_state:
        line = html.escape(line)
        fout.write(line+'\n')

    images = parse_binary_images(report)
    for line in images:
        line = html.escape(line)
        fout.write(line+'\n')

    extra = parse_extra_info(report)
    for line in extra:
        line = html.escape(line)
        fout.write(line+'\n')
    
    log_info = parse_log_info(report)
    for line in log_info:
        line = html.escape(line)
        fout.write(line+'\n')
    
if __name__ == '__main__':
    importlib.reload(sys)
    # sys.setdefaultencoding('utf-8')
    if len(sys.argv) != 3:
        print("usage: python3 %s json_file out_file" % sys.argv[0])
        sys.exit(1)

    report = json.load(open(sys.argv[1]))
    fout = open(sys.argv[2], 'w')
    if not isinstance(report, list):
        report = [report]
    
    for item in report:
        ks_json_2_apple(item, fout)
    fout.close()
