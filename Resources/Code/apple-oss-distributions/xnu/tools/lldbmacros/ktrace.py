from __future__ import absolute_import, division, print_function

from builtins import map
from builtins import range
from builtins import object

from xnu import *
from utils import *
from core.lazytarget import *
from misc import *
from kcdata import kcdata_item_iterator, KCObject, GetTypeForName, KCCompressedBufferObject
from collections import namedtuple
import heapq
import os
import plistlib
import struct
import subprocess
import sys
import tempfile
import time

# From the defines in bsd/sys/kdebug.h:

KdebugClassNames = {
    1: "MACH",
    2: "NETWORK",
    3: "FSYSTEM",
    4: "BSD",
    5: "IOKIT",
    6: "DRIVERS",
    7: "TRACE",
    8: "DLIL",
    9: "WORKQUEUE",
    10: "CORESTORAGE",
    11: "CG",
    20: "MISC",
    30: "SECURITY",
    31: "DYLD",
    32: "QT",
    33: "APPS",
    34: "LAUNCHD",
    36: "PPT",
    37: "PERF",
    38: "IMPORTANCE",
    39: "PERFCTRL",
    40: "BANK",
    41: "XPC",
    42: "ATM",
    43: "ARIADNE",
    44: "DAEMON",
    45: "ENERGYTRACE",
    49: "IMG",
    50: "CLPC",
    128: "ANS",
    129: "SIO",
    130: "SEP",
    131: "ISP",
    132: "OSCAR",
    133: "EMBEDDEDGFX"
}

def GetKdebugClassName(class_num):
    return (KdebugClassNames[class_num] + ' ({})'.format(class_num) if class_num in KdebugClassNames else 'unknown ({})'.format(class_num))

@lldb_type_summary(['typefilter_t'])
@header('{0: <20s}'.format("class") + ' '.join(map('{:02x}'.format, list(range(0, 255, 8)))))
def GetKdebugTypefilter(typefilter):
    """ Summarizes the provided typefilter.
    """
    classes = 256
    subclasses_per_class = 256

    # 8 bits at a time
    subclasses_per_element = 64
    cur_typefilter = cast(typefilter, 'uint64_t *')
    subclasses_fmts = ' '.join(['{:02x}'] * 8)

    elements_per_class = subclasses_per_class // subclasses_per_element

    out_str = ''
    for i in range(0, classes):
        print_class = False
        subclasses = [0] * elements_per_class

        # check subclass ranges for set bits, remember those subclasses
        for j in range(0, elements_per_class):
            element = unsigned(cur_typefilter[i * elements_per_class + j])
            if element != 0:
                print_class = True
            if print_class:
                subclasses[j] = element

        ## if any of the bits were set in a class, print the entire class
        if print_class:
            out_str += '{:<20s}'.format(GetKdebugClassName(i))
            for element in subclasses:
                # split up the 64-bit values into byte-sized pieces
                bytes = [unsigned((element >> i) & 0xff) for i in (0, 8, 16, 24, 32, 40, 48, 56)]
                out_str += subclasses_fmts.format(*bytes)
                out_str += ' '

            out_str += '\n'

    return out_str

@lldb_command('showkdebugtypefilter')
def ShowKdebugTypefilter(cmd_args=None):
    """ Show the current kdebug typefilter (or the typefilter at an address)

        usage: showkdebugtypefilter [<address>]
    """

    if cmd_args:
        typefilter = kern.GetValueFromAddress(cmd_args[0], 'typefilter_t')
        if unsigned(typefilter) == 0:
            raise ArgumentError('argument provided is NULL')

        print(GetKdebugTypefilter.header)
        print('-' * len(GetKdebugTypefilter.header))

        print(GetKdebugTypefilter(typefilter))
        return

    typefilter = kern.globals.kdbg_typefilter
    if unsigned(typefilter) == 0:
        raise ArgumentError('no argument provided and active typefilter is not set')

    print(GetKdebugTypefilter.header)
    print('-' * len(GetKdebugTypefilter.header))
    print(GetKdebugTypefilter(typefilter))

def GetKdebugStatus():
    """ Get a string summary of the kdebug subsystem.
    """
    out = ''

    kdebug_flags = kern.globals.kd_ctrl_page_trace.kdebug_flags
    out += 'kdebug flags: {}\n'.format(xnudefines.GetStateString(xnudefines.kdebug_flags_strings, kdebug_flags))
    events = kern.globals.kd_data_page_trace.nkdbufs
    buf_mb = events * (64 if kern.arch == 'x86_64' or kern.arch.startswith('arm64') else 32) // 1000000
    out += 'events allocated: {:<d} ({:<d} MB)\n'.format(events, buf_mb)
    out += 'enabled: {}\n'.format('yes' if kern.globals.kdebug_enable != 0 else 'no')
    if kdebug_flags & xnudefines.KDBG_TYPEFILTER_CHECK:
        out += 'typefilter:\n'
        out += GetKdebugTypefilter.header + '\n'
        out += '-' * len(GetKdebugTypefilter.header) + '\n'
        typefilter = kern.globals.kdbg_typefilter
        if unsigned(typefilter) != 0:
            out += GetKdebugTypefilter(typefilter)

    return out

@lldb_command('showkdebug')
def ShowKdebug(cmd_args=None):
    """ Show the current kdebug state.

        usage: showkdebug
    """

    print(GetKdebugStatus())

@lldb_type_summary(['kperf_timer'])
@header('{:<10s} {:<7s} {:<20s} {:<20s}'.format('period-ns', 'action', 'deadline', 'fire-time'))
def GetKperfTimerSummary(timer):
    """ Get a string summary of a kperf timer.

        params:
            timer: the kptimer object to get a summary of
    """
    try:
        fire_time = timer.kt_fire_time
    except:
        fire_time = 0
    return '{:<10d} {:<7d} {:<20d} {:<20d}\n'.format(
        kern.GetNanotimeFromAbstime(timer.kt_period_abs), timer.kt_actionid,
        timer.kt_cur_deadline, fire_time)

@lldb_type_summary(['action'])
@header('{:<10s} {:<20s} {:<20s}'.format('pid-filter', 'user-data', 'samplers'))
def GetKperfActionSummary(action):
    """ Get a string summary of a kperf action.

        params:
            action: the action object to get a summary of
    """
    samplers = xnudefines.GetStateString(xnudefines.kperf_samplers_strings, action.sample)
    return '{:<10s} {:<20x} {:<20s}\n'.format(
        '-' if action.pid_filter < 0 else str(action.pid_filter), action.userdata, samplers)

def GetKperfKdebugFilterDescription():
    kdebug_filter = kern.globals.kperf_kdebug_filter
    desc = ''
    for i in range(kdebug_filter.n_debugids):
        filt_index = 1 if i >= 16 else 0
        filt_type = (kdebug_filter.types[filt_index] >> ((i % 16) * 4)) & 0xf
        debugid = kdebug_filter.debugids[i]
        if filt_type < 2:
            prefix = 'C'
            width = 2
            id = debugid >> 24
        elif filt_type < 4:
            prefix = 'S'
            width = 4
            id = debugid >> 16
        else:
            prefix = 'D'
            width = 8
            id = debugid

        suffix = ''
        if (filt_type % 2) == 1:
            if debugid & xnudefines.DBG_FUNC_START:
                suffix = 's'
            elif debugid & xnudefines.DBG_FUNC_END:
                suffix = 'r'
            else:
                suffix = 'n'

        if i > 0:
            desc += ','
        desc += '{prefix}{id:0{width}x}{suffix}'.format(
                prefix=prefix, id=id, width=width, suffix=suffix)

    return desc

def GetKperfStatus():
    """ Get a string summary of the kperf subsystem.
    """
    out = ''

    kperf_status = int(kern.globals.kperf_status)
    out += 'sampling: '
    if kperf_status == GetEnumValue('kperf_sampling::KPERF_SAMPLING_OFF'):
        out += 'off\n'
    elif kperf_status == GetEnumValue('kperf_sampling::KPERF_SAMPLING_SHUTDOWN'):
        out += 'shutting down\n'
    elif kperf_status == GetEnumValue('kperf_sampling::KPERF_SAMPLING_ON'):
        out += 'on\n'
    else:
        out += 'unknown\n'

    pet = kern.globals.kptimer.g_pet_active
    pet_timer_id = kern.globals.kptimer.g_pet_active
    if pet != 0:
        pet_idle_rate = kern.globals.pet_idle_rate
        out += 'legacy PET is active (timer = {:<d}, idle rate = {:<d})\n'.format(pet_timer_id, pet_idle_rate)
    else:
        out += 'legacy PET is off\n'

    lw_pet = kern.globals.kppet.g_lightweight
    if lw_pet != 0:
        lw_pet_gen = kern.globals.kppet_gencount
        out += 'lightweight PET is active (timer = {:<d}, generation count = {:<d})\n'.format(pet_timer_id, lw_pet_gen)
    else:
        out += 'lightweight PET is off\n'

    actions = kern.globals.actionc
    actions_arr = kern.globals.actionv

    out += 'actions:\n'
    out += '{:<5s} '.format('id') + GetKperfActionSummary.header + '\n'
    for i in range(0, actions):
        out += '{:<5d} '.format(i) + GetKperfActionSummary(actions_arr[i])

    timers = kern.globals.kptimer.g_ntimers
    timers_arr = kern.globals.kptimer.g_timers

    out += 'timers:\n'
    out += '{:<5s} '.format('id') + GetKperfTimerSummary.header + '\n'
    for i in range(0, timers):
        out += '{:<5d} '.format(i) + GetKperfTimerSummary(timers_arr[i])

    return out


def GetKtraceStatus():
    """ Get a string summary of the ktrace subsystem.
    """
    out = ''

    state = kern.globals.ktrace_state
    if state == GetEnumValue('ktrace_state::KTRACE_STATE_OFF'):
        out += 'ktrace is off\n'
    else:
        out += 'ktrace is active ('
        if state == GetEnumValue('ktrace_state::KTRACE_STATE_FG'):
            out += 'foreground)'
        else:
            out += 'background)'
        out += '\n'
        owner = kern.globals.ktrace_last_owner_execname
        out += 'owned by: {0: <s} [%u]\n'.format(owner)
        active_mask = kern.globals.ktrace_active_mask
        out += 'active systems: {:<#x}\n'.format(active_mask)

    return out


def GetKtraceConfig():
    kdebug_state = 0
    if (kern.globals.kd_ctrl_page_trace.kdebug_flags & xnudefines.KDBG_BUFINIT) != 0:
        kdebug_state = 1
    if kern.globals.kdebug_enable:
        kdebug_state = 3
    kdebug_wrapping = True
    if (kern.globals.kd_ctrl_page_trace.kdebug_flags & xnudefines.KDBG_NOWRAP):
        kdebug_wrapping = False

    kperf_state = 3 if (
            unsigned(kern.globals.kperf_status) ==
            GetEnumValue('kperf_sampling::KPERF_SAMPLING_ON')) else 0

    action_count = kern.globals.actionc
    actions = kern.globals.actionv
    action_samplers = []
    action_user_datas = []
    action_pid_filters = []
    for i in range(action_count):
        action = actions[i]
        action_samplers.append(unsigned(action.sample))
        action_user_datas.append(unsigned(action.userdata))
        action_pid_filters.append(unsigned(action.pid_filter))

    timer_count = kern.globals.kptimer.g_ntimers
    timers = kern.globals.kptimer.g_timers
    timer_actions = []
    timer_periods_ns = []

    for i in range(timer_count):
        timer = timers[i]
        timer_actions.append(unsigned(timer.kt_actionid))
        timer_periods_ns.append(
            kern.GetNanotimeFromAbstime(unsigned(timer.kt_period_abs)))

    pet_mode = 0
    if kern.globals.kppet.g_lightweight:
        pet_mode = 2
    elif kern.globals.kptimer.g_pet_active:
        pet_mode = 1

    return {
        'owner_name': str(kern.globals.ktrace_last_owner_execname),
        'owner_kind': unsigned(kern.globals.ktrace_state),
        'owner_pid': int(kern.globals.ktrace_owning_pid),

        'kdebug_state': kdebug_state,
        'kdebug_buffer_size': unsigned(kern.globals.kd_data_page_trace.nkdbufs),
        'kdebug_typefilter': plistlib.Data(struct.pack('B', 0xff) * 4096 * 2), # XXX
        'kdebug_procfilt_mode': 0, # XXX
        'kdebug_procfilt': [], # XXX
        'kdebug_wrapping': kdebug_wrapping,

        'kperf_state': kperf_state,
        'kperf_actions_sampler': action_samplers,
        'kperf_actions_user_data': action_user_datas,
        'kperf_actions_pid_filter': action_pid_filters,
        'kperf_timers_action_id': timer_actions,
        'kperf_timers_period_ns': timer_periods_ns,
        'kperf_pet_mode': pet_mode,
        'kperf_pet_timer_id': unsigned(kern.globals.kptimer.g_pet_timerid),
        'kperf_pet_idle_rate': unsigned(kern.globals.kppet.g_idle_rate),

        'kperf_kdebug_action_id': unsigned(kern.globals.kperf_kdebug_action),
        'kperf_kdebug_filter': GetKperfKdebugFilterDescription(),

        # XXX
        'kpc_state': 0,
        'kpc_classes': 0,
        'kpc_thread_classes': 0,
        'kpc_periods': [],
        'kpc_action_ids': [],
        'kpc_config': [],

        'context_kind': 2, # backwards-facing
        'reason': 'core file debugging',
        'command': '(lldb) savekdebugtrace',
        'trigger_kind': 2, # diagnostics trigger
    }


@lldb_command('showktrace')
def ShowKtrace(cmd_args=None):
    """ Show the current ktrace state, including subsystems.

        usage: showktrace
    """

    print(GetKtraceStatus())
    print(' ')
    print('kdebug:')
    print(GetKdebugStatus())
    print(' ')
    print('kperf:')
    print(GetKperfStatus())


class KDEvent(object):
    """
    Wrapper around kevent pointer that handles sorting logic.
    """
    def __init__(self, timestamp, kevent, k64):
        self.kevent = kevent
        self.timestamp = timestamp
        self.k64 = k64

    def get_kevent(self):
        return self.kevent

    def __eq__(self, other):
        return self.timestamp == other.timestamp

    def __lt__(self, other):
        return self.timestamp < other.timestamp

    def __gt__(self, other):
        return self.timestamp > other.timestamp

    def __hash__(self):
        return hash(self.timestamp)


class KDCPU(object):
    """
    Represents all events from a single CPU.
    """
    def __init__(self, cpuid, k64, verbose):
        self.cpuid = cpuid
        self.iter_store = None
        self.k64 = k64
        self.verbose = verbose
        self.timestamp_mask = ((1 << 48) - 1) if not self.k64 else ~0
        self.last_timestamp = 0

        kdstoreinfo = kern.globals.kd_data_page_trace.kdbip[cpuid]
        self.kdstorep = kdstoreinfo.kd_list_head

        if self.kdstorep.raw == xnudefines.KDS_PTR_NULL:
            # Returns an empty iterrator. It will immediatelly stop at
            # first call to __next__().
            return

        self.iter_store = self.get_kdstore(self.kdstorep)

        # XXX Doesn't have the same logic to avoid un-mergeable events
        #     (respecting barrier_min and bufindx) as the C code.

        self.iter_idx = self.iter_store.kds_readlast

    def get_kdstore(self, kdstorep):
        """
        See POINTER_FROM_KDSPTR.
        """
        buf = kern.globals.kd_data_page_trace.kd_bufs[kdstorep.buffer_index]
        return addressof(buf.kdsb_addr[kdstorep.offset])

    # Event iterator implementation returns KDEvent instance

    def __iter__(self):
        return self

    def __next__(self):
        # This CPU is out of events
        if self.iter_store is None:
            raise StopIteration

        if self.iter_idx == self.iter_store.kds_bufindx:
            self.iter_store = None
            raise StopIteration

        keventp = addressof(self.iter_store.kds_records[self.iter_idx])
        timestamp = unsigned(keventp.timestamp) & self.timestamp_mask
        if self.last_timestamp == 0 and self.verbose:
            print('first event from CPU {} is at time {}'.format(
                    self.cpuid, timestamp))
        self.last_timestamp = timestamp

        # check for writer overrun
        if timestamp < self.iter_store.kds_timestamp:
            raise StopIteration

        # Advance iterator
        self.iter_idx += 1

        if self.iter_idx == xnudefines.EVENTS_PER_STORAGE_UNIT:
            snext = self.iter_store.kds_next
            if snext.raw == xnudefines.KDS_PTR_NULL:
                # Terminate iteration in next loop. Current element is the
                # last one in this CPU buffer.
                self.iter_store = None
            else:
                self.iter_store = self.get_kdstore(snext)
                self.iter_idx = self.iter_store.kds_readlast

        return KDEvent(timestamp, keventp, self.k64)

    # Python 2 compatibility
    # pragma pylint: disable=next-method-defined
    def next(self):
        return self.__next__()


def IterateKdebugEvents(verbose=False):
    """
    Yield events from the in-memory kdebug trace buffers.
    """
    ctrl = kern.globals.kd_ctrl_page_trace

    if (ctrl.kdebug_flags & xnudefines.KDBG_BUFINIT) == 0:
        return

    barrier_min = ctrl.oldest_time

    if (ctrl.kdebug_flags & xnudefines.KDBG_WRAPPED) != 0:
        # TODO Yield a wrap event with the barrier_min timestamp.
        pass

    k64 = kern.ptrsize == 8
    # Merge sort all events from all CPUs.
    cpus = [KDCPU(cpuid, k64, verbose) for cpuid in range(ctrl.kdebug_cpus)]
    last_timestamp = 0
    warned = False
    for event in heapq.merge(*cpus):
        if event.timestamp < last_timestamp and not warned:
            print('warning: events seem to be out-of-order')
            warned = True
        last_timestamp = event.timestamp
        yield event.get_kevent()


def GetKdebugEvent(event, k64):
    """
    Return a string representing a kdebug trace event.
    """
    if k64:
        timestamp = event.timestamp & ((1 << 48) - 1)
        cpuid = event.timestamp >> 48
    else:
        timestamp = event.timestamp
        cpuid = event.cpuid

    return '{:16} {:8} {:8x} {:16} {:16} {:16} {:16} {:4} {:8} {}'.format(
            unsigned(timestamp), 0, unsigned(event.debugid),
            unsigned(event.arg1), unsigned(event.arg2),
            unsigned(event.arg3), unsigned(event.arg4), unsigned(cpuid),
            unsigned(event.arg5), "")


@lldb_command('showkdebugtrace')
def ShowKdebugTrace(cmd_args=None):
    """
    List the events present in the kdebug trace buffers.

    (lldb) showkdebugtrace

    Caveats:
        * Events from IOPs may be missing or cut-off -- they weren't informed
          of this kind of buffer collection.
    """
    k64 = kern.ptrsize == 8
    for event in IterateKdebugEvents(config['verbosity'] > vHUMAN):
        print(GetKdebugEvent(event, k64))


def binary_plist(o):
    with tempfile.NamedTemporaryFile(delete=False) as f:
        plistlib.writePlist(o, f)
        name = f.name

    subprocess.check_output(['plutil', '-convert', 'binary1', name])
    with open(name, mode='rb') as f:
        plist = f.read()

    os.unlink(name)
    return plist


def align_next_chunk(f, offset, verbose):
    trailing = offset % 8
    padding = 0
    if trailing != 0:
        padding = 8 - trailing
        f.write(b'\x00' * padding)
        if verbose:
            print('aligned next chunk with {} padding bytes'.format(padding))
    return padding


def GetKtraceMachine():
    """
    This is best effort -- several fields are only available to user space or
    are difficult to determine from a core file.
    """
    master_cpu_data = GetCpuDataForCpuID(0)
    if kern.arch == 'x86_64':
        cpu_family = unsigned(kern.globals.cpuid_cpu_info.cpuid_cpufamily)
    else:
        cpu_family = 0 # XXX

    k64 = kern.ptrsize == 8
    page_size = 4 * 4096 if k64 else 4096

    return {
        'kern_version': str(kern.globals.version),
        'boot_args': '', # XXX
        'hw_memsize': unsigned(kern.globals.max_mem),
        'hw_pagesize': page_size, # XXX
        'vm_pagesize': page_size, # XXX
        'os_name': 'Unknown', # XXX
        'os_version': 'Unknown', # XXX
        'os_build': str(kern.globals.osversion),
        'arch': 'Unknown', # XXX
        'hw_model': 'Unknown', # XXX
        'cpu_type': unsigned(master_cpu_data.cpu_type),
        'cpu_subtype': unsigned(master_cpu_data.cpu_subtype),
        'cpu_family': cpu_family,
        'active_cpus': unsigned(kern.globals.processor_avail_count),
        'max_cpus': unsigned(kern.globals.machine_info.logical_cpu_max),
        'apple_internal': True, # XXX
    }


CHUNKHDR_PACK = 'IHHQ'


def append_chunk(f, file_offset, tag, major, minor, data, verbose):
    f.write(struct.pack(CHUNKHDR_PACK, tag, major, minor, len(data)))
    f.write(data)
    offset = 16 + len(data)
    return offset + align_next_chunk(f, file_offset + offset, verbose)


@lldb_command('savekdebugtrace', 'N:M')
def SaveKdebugTrace(cmd_args=None, cmd_options={}):
    """
    Save any valid ktrace events to a file.

    (lldb) savekdebugtrace [-M] [-N <n-events>] <file-to-write>

        -N <n-events>: only save the last <n-events> events
        -M: ensure output is machine-friendly

    Caveats:
        * 32-bit kernels are unsupported.
        * Fewer than the requested number of events may end up in the file.
        * The machine and config chunks may be missing crucial information
          required for tools to analyze them.
        * Events from IOPs may be missing or cut-off -- they weren't informed
          of this kind of buffer collection.
    """

    k64 = kern.ptrsize == 8

    if len(cmd_args) != 1:
        raise ArgumentError('error: path to trace file is required')

    nevents = unsigned(kern.globals.kd_data_page_trace.nkdbufs)
    if nevents == 0:
        print('error: kdebug buffers are not set up')
        return

    limit_nevents = nevents
    if '-N' in cmd_options:
        limit_nevents = unsigned(cmd_options['-N'])
        if limit_nevents > nevents:
            limit_nevents = nevents
    verbose = config['verbosity'] > vHUMAN
    humanize = '-M' not in cmd_options

    file_offset = 0
    with open(cmd_args[0], 'w+b') as f:
        FILE_MAGIC = 0x55aa0300
        EVENTS_TAG = 0x00001e00
        SSHOT_TAG = 0x8002
        CONFIG_TAG = 0x8006
        MACHINE_TAG = 0x8c00
        CHUNKHDR_PACK = 'IHHQ'
        FILEHDR_PACK = CHUNKHDR_PACK + 'IIQQIIII'
        FILEHDR_SIZE = 40
        FUTURE_SIZE = 8

        numer, denom = GetTimebaseInfo()

        # XXX The kernel doesn't have a solid concept of the wall time.
        wall_abstime = 0
        wall_secs = 0
        wall_usecs = 0

        # XXX 32-bit is NYI
        event_size = 64 if k64 else 32

        file_hdr = struct.pack(
                FILEHDR_PACK, FILE_MAGIC, 0, 0, FILEHDR_SIZE,
                numer, denom, wall_abstime, wall_secs, wall_usecs, 0, 0,
                0x1 if k64 else 0)
        f.write(file_hdr)
        header_size_offset = file_offset + 8
        file_offset += 16 + FILEHDR_SIZE # chunk header plus file header

        if verbose:
            print('writing machine chunk at offset 0x{:x}'.format(file_offset))
        machine_data = GetKtraceMachine()
        machine_bytes = binary_plist(machine_data)
        file_offset += append_chunk(
                f, file_offset, MACHINE_TAG, 0, 0, machine_bytes, verbose)

        if verbose:
            print('writing config chunk at offset 0x{:x}'.format(file_offset))
        config_data = GetKtraceConfig()
        config_bytes = binary_plist(config_data)
        file_offset += append_chunk(
                f, file_offset, CONFIG_TAG, 0, 0, config_bytes, verbose)

        events_hdr = struct.pack(
                CHUNKHDR_PACK, EVENTS_TAG, 0, 0, 0) # size will be filled in later
        f.write(events_hdr)
        file_offset += 16 # header size
        event_size_offset = file_offset - FUTURE_SIZE
        # Future events timestamp -- doesn't need to be set for merged events.
        f.write(struct.pack('Q', 0))
        file_offset += FUTURE_SIZE

        if verbose:
            print('events start at offset 0x{:x}'.format(file_offset))

        process = LazyTarget().GetProcess()
        error = lldb.SBError()

        skip_nevents = nevents - limit_nevents if limit_nevents else 0
        if skip_nevents > 0:
            print('omitting {} events from the beginning'.format(skip_nevents))

        written_nevents = 0
        seen_nevents = 0
        start_time = time.time()
        update_every = 1000 if humanize else 25000
        for event in IterateKdebugEvents(verbose):
            seen_nevents += 1
            if skip_nevents >= seen_nevents:
                if seen_nevents % update_every == 0:
                    sys.stderr.write('skipped {}/{} ({:4.2f}%) events'.format(
                            seen_nevents, skip_nevents,
                            float(seen_nevents) / skip_nevents * 100.0))
                    sys.stderr.write('\r')

                continue

            event = process.ReadMemory(
                    unsigned(event), event_size, error)
            file_offset += event_size
            f.write(event)
            written_nevents += 1
            # Periodically update the CLI with progress.
            if written_nevents % update_every == 0:
                sys.stderr.write('{}: wrote {}/{} ({:4.2f}%) events'.format(
                        time.strftime('%H:%M:%S'), written_nevents,
                        limit_nevents,
                        float(written_nevents) / nevents * 100.0))
                if humanize:
                    sys.stderr.write('\r')
                else:
                    sys.stderr.write('\n')
        sys.stderr.write('\n')
        elapsed = time.time() - start_time
        print('wrote {} ({}MB) events in {:.3f}s'.format(
                written_nevents, written_nevents * event_size >> 20, elapsed))
        if verbose:
            print('events end at offset 0x{:x}'.format(file_offset))

        # Normally, the chunk would need to be padded to 8, but events are
        # already aligned.

        kcdata = kern.globals.kc_panic_data
        kcdata_addr = unsigned(kcdata.kcd_addr_begin)
        kcdata_length = unsigned(kcdata.kcd_length)
        if kcdata_addr != 0 and kcdata_length != 0:
            print('writing stackshot')
            if verbose:
                print('stackshot is 0x{:x} bytes at offset 0x{:x}'.format(
                        kcdata_length, file_offset))
            ssdata = process.ReadMemory(kcdata_addr, kcdata_length, error)
            magic = struct.unpack('I', ssdata[:4])
            if magic[0] == GetTypeForName('KCDATA_BUFFER_BEGIN_COMPRESSED'):
                if verbose:
                    print('found compressed stackshot')
                iterator = kcdata_item_iterator(ssdata)
                for item in iterator:
                    kcdata_buffer = KCObject.FromKCItem(item)
                    if isinstance(kcdata_buffer, KCCompressedBufferObject):
                        kcdata_buffer.ReadItems(iterator)
                        decompressed = kcdata_buffer.Decompress(ssdata)
                        ssdata = decompressed
                        kcdata_length = len(ssdata)
                        if verbose:
                            print(
                                    'compressed stackshot is 0x{:x} bytes long'.
                                    format(kcdata_length))

            file_offset += append_chunk(
                    f, file_offset, SSHOT_TAG, 1, 0, ssdata, verbose)
            if verbose:
                print('stackshot ends at offset 0x{:x}'.format(file_offset))
        else:
            print('warning: no stackshot; trace file may not be usable')

        # After the number of events is known, fix up the events chunk size.
        events_data_size = unsigned(written_nevents * event_size) + FUTURE_SIZE
        f.seek(event_size_offset)
        f.write(struct.pack('Q', events_data_size))
        if verbose:
            print('wrote {:x} bytes at offset 0x{:x} for event size'.format(
                    events_data_size, event_size_offset))

        # Fix up the size of the header chunks, too.
        f.seek(header_size_offset)
        f.write(struct.pack('Q', FILEHDR_SIZE + 16 + len(machine_bytes)))
        if verbose:
            print((
                    'wrote 0x{:x} bytes at offset 0x{:x} for ' +
                    'file header size').format(
                    len(machine_bytes), header_size_offset))

    return
