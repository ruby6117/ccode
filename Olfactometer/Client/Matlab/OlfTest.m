function [] = OlfTest(host)
% OlfTest -- Run a series of tests on an olfactometer
% 
% OlfTest('192.168.0.178') would run tests on the olfactometer
% connected to 192.168.0.178

  olf = Olfactometer(host);
  
  s = struct ('olf', olf, 'stop', 0);
  
  % to cancel
  fig = figure;
  set(fig, 'UserData', s);
  set(fig, 'Name', 'OlfTest Window');
  set(fig, 'DockControls', 'Off');
  set(fig, 'ToolBar', 'None');
  set(fig, 'MenuBar', 'None');
  set(fig, 'Position', [118   120   174   106]);
  set(fig, 'DeleteFcn', @ui_delete);
  but = uicontrol(fig, 'Style', 'pushbutton', 'ButtonDownFcn', ...
                  @ui_cb, 'Callback', @ui_cb);
  set(but, 'Position', [10 10 50 25], 'String', 'Cancel');
  % Modify these variables to change the testing...
  %
  % Testing is as follows:  For each totalFlow, each odorFlow is
  % done, cycling through the odors.  All flows and odors are
  % applied to all mixes and banks.  Additionally, for each of the
  % above, mixRatios cycled through.  The delay between each change
  % is delaySeconds
  %
  totalFlows = [ 1000 950 900 ];
  
  nTotalFlows = size(totalFlows,2);

  % each of the odorFlows below gets applied to each of the total
  % flows above
  odorFlows = [ 100 50 25 ];

  nOdorFlows = size(odorFlows, 2);
  
  
  % for each of the two above, each of these mixRatios is applied
  mixRatios =  [ 1 1 ; ...
                 1 2 ; ...
                 60 40;  ...
                 90 10; ...
                 10 90; ];
  nMixRatios = size(mixRatios, 1);
  
  % for each of the mixratios above, each odor below is applied
  odors = [ 0 1 0 2 0 3 0 4 0 5 0 6 0 7 0 8 0 9 0 10 0 11 0 12 0 13 ...
            0 14 0 15 0 ];
  nOdors = size(odors, 2);

  % amount of time in seconds to wait for each step
  delay = 1.0; 
  
  mixes = GetMixNames(olf);  nMixes = size(mixes, 1);
  banks = GetBankNames(olf); nBanks = size(banks, 1);
  
  for t=1:nTotalFlows
    totalFlow = totalFlows(1,t);
    for o=1:nOdorFlows
      odorFlow = odorFlows(1,o);
      for m=1:nMixRatios
        mixRatio = mixRatios(m,1:size(mixRatios,2));
        str = '';
        strDone = 0;
        for mx=1:nMixes
          mix = mixes{mx};          
          mr = GetMixRatio(olf, mix);
          for mri=1:size(mr, 1)
            if (mri <= size(mixRatio, 2))
              mr{mri, 2} = mixRatio(mri);
              if (~strDone)
                str = sprintf('%s %d', str, mr{mri, 2});
              end;
            end;
          end;
          strDone = 1;
          olf = SetMixRatio(olf, mix, mr);
          olf = SetOdorFlow(olf, mix, odorFlow);
          olf = SetTotalFlow(olf, mix, totalFlow);
        end;
        disp(sprintf('Set mix ratio to %s', str));
        disp(sprintf('Set odor flows to %d', odorFlow));
        disp(sprintf('Set total flow to %d', totalFlow));
        for odr=1:size(odors,2)
          odor = odors(1,odr);
          disp(sprintf('Switch to odor %d', odor));
          currOdors = get(olf, 'BankCurrentOdors');
          for i=1:size(currOdors,1)
            currOdors(i) = odor;
          end;
          olf = set(olf, 'BankCurrentOdors', currOdors);
          disp(sprintf('Pausing %d secs..\n', delay));
          pause(delay);
          try 
              s = get(fig, 'UserData');
          catch 
              disp('Figure deleted, aborting...');
              return;
          end;
          olf = s.olf;
          if (s.stop), 
              disp('User stop requested..');
              Close(olf); 
              delete(fig); 
              return; 
          end;
        end;
      end;
    end;
  end;
  
  Close(olf);
  delete(fig);
  
  
function [] = ui_cb(sender, eventdata)

    fig = get(sender, 'Parent');
    s = get(fig, 'UserData');
    s.stop = 1;
    set(fig,'UserData', s);
  
  
function [] = ui_delete(sender, eventdata)
        
    s = get(sender, 'UserData');
    if (~s.stop),  s.olf = Close(s.olf); end;
    s.stop = 1;
    set(sender,'UserData', s);
  
  
  
  
  
  
  


