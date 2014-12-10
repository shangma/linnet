function [sys] = createLtiSystem(sysDesc)

%   createLtiSystem() - Create an Octave LTI system of sub-class tf.
%                        
%   Input argument(s): 
%      sysDesc         A struct that describes all relevant properties of the system
%
%   Return argument(s):
%      sys             A single LTI sytem is returned if the number of system outputs is not
%                      too large. Otherwise the MIMO system with m in- and p outputs is
%                      split into p MISO systems with m inputs. In this case all MISO
%                      systems are returned in a cell array.
% 
%   Example(s):
%      sysDesc = struct( 'name', 'sys' ...
%                      , 'numeratorAry', {{[1 5 7] [1]; [1 7] [1 -5 -5]}} ...
%                      , 'denominatorAry', {{[1 5 6] [2]; [1 3 1] [8 0 1]}} ...
%                      , 'inputNameAry', {{'inp_u1' 'inp_u2'}} ...
%                      , 'outputNameAry', {{'out_y1' 'out_y2'}} ...
%                      );
%      sys = createLtiSystem(sysDesc)
%
%   Copyright (C) 2014 Peter Vranken (mailto:Peter_Vranken@Yahoo.de)
%   
%   This program is free software: you can redistribute it and/or modify it
%   under the terms of the GNU General Public License as published by the
%   Free Software Foundation, either version 3 of the License, or (at your
%   option) any later version.
%   
%   This program is distributed in the hope that it will be useful, but
%   WITHOUT ANY WARRANTY; without even the implied warranty of
%   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
%   General Public License for more details.
%   
%   You should have received a copy of the GNU General Public License along
%   with this program. If not, see <http://www.gnu.org/licenses/>.

    % Number of mandatory parameters.
    noPar = 1;
    
    % Number of optional parameters.
    noOptPar = 0;
    
    error(nargchk(noPar, noPar+noOptPar, nargin));

%    % Set the optional parameter values.
%    noPar = noPar + 1;
%    if nargin < noPar
%        optPar1 = ;
%    end

    if isOctave
        pkg load control
    end
    
    % Octave 3.4.3 (control 2.2.0) only supports SISO systems for the plot function bode.
    % But even with the more flexible MATLAB it's useless to have too many figures in one
    % window. Everything gets too small.
    if isOctave
        maxOutputs = 8;
    else
        maxOutputs = 8;
    end
    
    [noY noU] = size(sysDesc.numeratorAry);

    if noY > maxOutputs
        error('Spliting systems not yet supported');
    else
        sys = tf(sysDesc.numeratorAry, sysDesc.denominatorAry);
        if isOctave
            sys = set( sys                                  ...
                     , 'name', sysDesc.name                 ...
                     , 'inname', sysDesc.inputNameAry       ...
                     , 'outname', sysDesc.outputNameAry     ...
                     );
        else
            %set(0, 'DefaultTextInterpreter', 'none');
            set( sys                                  ...
               , 'Name', sysDesc.name                 ...
               , 'InputName', sysDesc.inputNameAry    ...
               , 'OutputName', sysDesc.outputNameAry  ...
               );
            %set(sys);
        end
    end

end % of function createLtiSystem.


