function [isOct] = isOctave

%   isOctave() - Distinguish between Octave and the commercial product MATLAB.
%                   Use this function to equalize differences between the products in your
%                   script code so that the scripts run in both environments.
%
%   Input argument(s):
%
%   Return argument(s):
%       isOct       true if this script is running in an Octave system, otherwise false
%
%   Example(s):
%       if isOctave, disp('This is Octave!'), end
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

    v = ver('Octave');
    isOct = numel(v) >= 1;

end % of function isOctave.


