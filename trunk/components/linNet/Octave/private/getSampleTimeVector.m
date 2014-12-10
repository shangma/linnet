function [t] = getSampleTimeVector(systemDesc)

%   getSampleTimeVector() - Compute a sample time vector for a step response plot.
%                   The sample time vector is shaped in accordance with a plot information
%                   object, which specifies the relevant frequency range of the LTI system
%                   and the number of points and their distribution. The sample time range
%                   is derived from the frequency range.
%
%   Input argument(s):
%       systemDesc  The description of the LTI system a Bode plot is to be prepared for. It
%                   contains a plot information object
%
%   Return argument(s):
%       t ......... The sample time vector. It may be the empty vector if the plot information
%                   tells to use default settings
%
%   Example(s):
%       t = getSampleTimeVector(systemDesc_myBodePlot);
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
        % Possible errors due to too few points, negative values are not handled; Ocatve
        % will provide sufficient feedback in case.
        t = linspace(0, 1/2/pi/systemDesc.plotInfo.freqMin, systemDesc.plotInfo.noPoints);
    else
        % Return the empty set, which will make step find the suitable sample time range and
        % resolution.
        t = [];
    end
end % of function getSampleTimeVector.


