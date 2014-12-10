function [w] = getFrequencyVector(systemDesc)

%   getFrequencyVector() - Compute a frequency vector for a Bode plot.
%                   The frequency vector is shaped in accordance with a plot information
%                   object, which specifies the frequency range and the number of points and
%                   their distribution.
%                   
%   Input argument(s):
%       systemDesc  The description of the LTI system a Bode plot is to be prepared for. It
%                   contains a plot information object
%
%   Return argument(s):
%       w ......... The frequency vector. It may be the empty vector if the plot information
%                   tells to use default settings
%
%   Example(s):
%       w = getFrequencyVector(systemDesc_myBodePlot);
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

    if ~isempty(systemDesc.plotInfo)
        if systemDesc.plotInfo.isLogX
            % Possible errors due to too few points, negative values and the pathologic case
            % freqMax=10^pi are not handled; Ocatve will provide sufficient feedback in case.
            w = logspace( log10(systemDesc.plotInfo.freqMin) ...
                        , log10(systemDesc.plotInfo.freqMax) ...
                        , systemDesc.plotInfo.noPoints       ...
                        );
        else
            warning(['The plot specification tells to use a linear distribution of' ...
                     ' points. However, the computed linear distribution of frequency' ...
                     ' points does not automatically mean that the plotting function' ...
                     ' will draw a linear axis. Plots with linear frequency axis' ...
                     ' are hence not fully supported'] ...
                   );
            w = linspace( systemDesc.plotInfo.freqMin        ...
                        , systemDesc.plotInfo.freqMax        ...
                        , systemDesc.plotInfo.noPoints       ...
                        );
        end
        
        % The plot information uses Hz for the frequency axis whereas Octave expects radian.
        w = 2*pi.*w;
    else
        % Return the empty set, which will make bode find the suitable frequency range.
        w = [];
    end
end % of function getFrequencyVector.


