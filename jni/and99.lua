icount = 0
branches = { _ptr = 0 }
call_stack = 10

-- this callback gets call on every instruction execution
-- takes the decoded instruction ID as input
-- callbacks.cpu = function (i)
--    icount = icount + 1
-- end

-- this callback gets called on every VDP write.
-- takes the address and the value being written
callbacks.vdp = function (a,v)
    if a == 547 and v == 102 then _break_cpu() end
end

-- this callback gets called on every taken branch with the new PC
callbacks.branch = function (newpc)
    branches[branches._ptr] = { _get_pc(), newpc }
    branches._ptr = (branches._ptr + 1) % call_stack
end

-- some helper functions
function dump_mem(m, start, len)
    for i = start, start+len do
        output(string.format("0x%02x ", m[i]))
    end
end

function bt()
    p = branches._ptr + call_stack - 1
    for i=p, p-call_stack+1, -1 do
        index = i % call_stack
        output(string.format("0x%x => 0x%x\n", branches[index][1], branches[index][2]))
    end
end
.
