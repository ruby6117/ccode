% function [res] = Write(olf, component, val)
%    Write a 'cooked' value to a component named 'component'
%    See also WriteRaw and List methods.
function [res] = Write(olf, component, val)
    res = DoSimpleCmd(olf, sprintf('WRITE %s %d', component, val));
    