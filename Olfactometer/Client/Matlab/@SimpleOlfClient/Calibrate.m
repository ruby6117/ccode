function [ res ] = Calibrate( varargin )
%CALIBRATE Calibrate an Olfactometer component.
%   This function takes 3-9 arguments.  Only the first 3 argumetns are 
%   required. Arguments are position-dependent.  Attempts to calibrate comp 
%   using reference calib_ref.  Returns a table suitable to pass to the 
%
%   See also SimpleOlfClient/SetCalib, SimpleOlfClient/GetCalib
%    
%SYNOPSIS
%   Calibrate(olf, comp, calib_ref, start_v, end_v, step_v, step_secs, step_samps, heuristic);
%
%ARGUMENT EXPLANATION
%   Only the first 3 args are required.
%
%   olf       : the olfactometer object
%   comp      : the component to calibrate
%   calib_ref : the reference component (the one whose sensor we use for
%               the calibration)
%   start_v   : optional start voltage for the actuator of 'comp'.  Defaults
%               to 0.
%   end_v     : optional end voltage for the actuator of 'comp'.  Defaults
%               to 5.
%   step_v    : optional voltage increment to use for each calibration
%               iteration.  Defaults to (end_v-start_v)/100
%   step_secs : the number of seconds to spend at each step of the
%               calibration.  Defaults to 5.  Supply a sufficient number
%               to allow for stabilization of the flow valve.
%   step_samps: the number of samples to average together for each step.
%               Defaults to 10.  Specify enough samples in order to allow for
%               accounting for noise, etc.
%   heuristic : A number greater than, equal to, or less than zero which
%               means to use the minimum, average, or maximum recorded
%               voltage from the component sensor for any particular calibration step.
res = [];

if (nargin < 3 | nargin > 9),
    error('Please pass 3-9 args to this function.');
    return;
end;

olf = varargin{1};
comp = varargin{2};
calib_ref = varargin{3};
if (nargin > 3), start_v = varargin{4};
else start_v = 0;
end;
if (nargin > 4), end_v = varargin{5};
else end_v = 5;
end;
if (nargin > 5), step_v = varargin{6};
else step_v = (end_v-start_v) / 100;
end;
if (nargin > 6), step_secs = varargin{7};
else step_secs = 5;
end;
if (nargin > 7), step_samps = varargin{8};
else step_samps = 10;
end;
if (nargin > 8), heuristic = varargin{9};
else heuristic = 0;
end;
    
% validate first arg
if ~isa(olf, 'SimpleOlfClient'),
    error('First argument must be an object of type @SimpleOlfClient!');
    return;
end;

% determine if second arg, comp, is valid
calibs = List(olf, 'calib');
[m,n] = size(calibs);
foundit = 0;
for i=1:m,
    if (strcmp(calibs{i}, comp)),
        foundit = 1;
        break;
    end;
end;    
if (~foundit),
    error(sprintf('%s is not a valid calibrateable component!', comp));
    return;
end;

% determing if third arg, calib_ref is valid
readables = List(olf, 'read');
[m, n] = size(readables);
foundit = 0;
for i=1:m,
    if (strcmp(readables{i}, calib_ref)),
        foundit = 1;
        break;
    end;
end;    
if (~foundit),
    error(sprintf('%s is not a valid calibration reference!', calib_ref));
    return;
end;


% determing if the componend was running for later..
wasrunning = 0;
try 
    wasrunning = IsRunning(olf, comp);
catch
    % nothing.. means comp happens to not be runnable which is ok
end;

try 
    Stop(olf, comp);
catch
    % nothing..
end;

calibmat = zeros(0, 2);

disp(sprintf(['Calibrating ''%s'' using reference ''%s''. Writing voltages\n' ...
              'from %f-%fV with an increment of %fV and spending %f seconds\n' ...
              'at each step.  At each step, taking %f samples which will be\n' ...
              'averaged together!\n'], ...
             comp, calib_ref, start_v, end_v, step_v, step_secs, step_samps));
for v=start_v:step_v:end_v,
    disp(sprintf('V=%f, waiting %f secs...', v, step_secs));
    if (~WriteRaw(olf, comp, v)),
        warning(sprintf('Write of %fV to %s failed for some reason!\n', v, comp));
        continue;
    end;
    pause(step_secs);
    disp('Calibrating..');
    vS = zeros(0, 1);
    flows = zeros(0, 1);
    for i=1:step_samps,
        pause(0.1);
        vS = [ vS; ReadRaw(olf, comp) ];
        flows = [ flows; Read(olf, calib_ref); ];        
    end;
    % take into account the 'heuristic' arg
    if (heuristic < 0),
        argV = min(vS);
    elseif (heuristic > 0),
        argV = max(vS);
    else
        argV = mean(vS);
    end;
    avgV = mean(vS);
    avgFlow = mean(flows);
    calibmat = [ calibmat; avgV, avgFlow ];
    disp(sprintf('collected %d points for average of %fV -> %f flow\n', step_samps, avgV, avgFlow));
end;

if (wasrunning), %restore it to running if it was running previously
    Start(olf, comp);
end;

res = calibmat;

return;

