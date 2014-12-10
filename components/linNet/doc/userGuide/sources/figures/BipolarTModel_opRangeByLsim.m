%   BipolarTModel_opRangeByLsim() - This script runs a simulation of the
%   found LTI object in order to find the operating range of the amplifier
%   circuit.
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

% We operate in the audio frequency range and will use a input signal of
% 1 kHz. This allows to do the needed amplitude modulation within one
% second. A sampling period of 50us is sufficient, we see 20 samples per
% period.
tiEndInit = 0.5;
tiEnd = tiEndInit + 1;
tiSample = 50e-6;
tiSim = [0:tiSample:tiEnd].';
noSamples = length(tiSim);

% The battery voltage and the base-emitter voltage drop are modelled as
% constants.
U_batt = ones(noSamples, 1) * 12;
U_be = ones(noSamples, 1) * 0.6;

% The LF input signal is modelled as a sinoid of 1 kHz with a rising
% amplitude. The first 500ms the input is null: This way we await all
% transients from switching U_batt and U_be from initially null to their
% constant values. (The better approach would be to specify the initial
% conditions for the internal states of the LTI object but this is much
% more difficult.)
noInitSamples = ceil(tiEndInit/tiSample);
noSineSamples = noSamples - noInitSamples;
U_in_init = zeros(noInitSamples, 1);
U_in_f = 1000;
U_in_sine = sin(2*pi*U_in_f*tiSim(1:noSineSamples));

% The modulation function of the input should rise from null to the final
% value. The final value should be somewhat above the expected maximum
% input amplitude. This maximum amplitude can be approximated: We have an
% amplification of about 100 and the maximum output amplitude is about
% half the battery voltage.
F_mod = (1:noSineSamples).'/noSineSamples * 1.5 * (12/2)/100;
U_in_sine = U_in_sine .* F_mod;

% Compose the LF input signal from the segments.
U_in = [U_in_init; U_in_sine];
assert(all(size(U_in) == [noSamples 1]));

% Compose the system input from the three input signals.
U = [U_batt U_in U_be];

% Create the LTI system object. Use the linNet generated function DC to do
% so.
[tfDC par] = DC

% The LTI system DC is: U_batt, U_in, U_be -> U_c, U_base, I_batt
%   Run the simulation.
Y = lsim(tfDC, U, tiSim);

% Extract samples of interest.
%   Side note: The initialization phase is not only out of scope. It is
% furthermore wrong if we expect to see the behavior of the true circuit.
% The true circuit starts up with strong non linearities while the linNet
% solution is linear all the time. The voltage at the transistor's base
% rises slowly from null due to input voltage null and the input capacitor
% C_in. In the true system the base current is also null and hence the
% collector current. In the linNet solution the base current has a
% negative start value (since U_be already has its positive, constant
% value) and hence (strictly linear behavior!) the collector current is
% strongly negative. Consequently the collector voltage shows a large
% positive start value, significantly above the battery voltage.
tiPlot = tiSim(noInitSamples+1:end);
U_c = Y(noInitSamples+1:end,1);
U_base = Y(noInitSamples+1:end,2);
I_batt = Y(noInitSamples+1:end,3);

% Plot the result
figure
hold on;
plot(tiPlot, [U_c U_in(noInitSamples+1:end)]);
plot([tiPlot(1) tiPlot(end)].', [0.6; 0.6].', 'c');
plot([tiPlot(1) tiPlot(end)].', [12; 12].', 'y');
title('U_c in dependency of U_in');
xlabel('time [s]');
ylabel('');
legend('U_c', 'U_in', 'U_c_min', 'U_c_max', 'location', 'eastoutside');
grid on
zoom on
