function [systemName] = environment

%   environment() - Get the name of the environment the current script is running on
%                   
%   Input argument(s):
%
%   Return argument(s):
%       systemName  The name of the interpreter, Octave or MATLAB
%
%   Example(s):
%       environment
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

    if isOctave
        systemName = 'Octave';
    else
        systemName = 'MATLAB';
    end
end % of function environment.


