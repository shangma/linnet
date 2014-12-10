% Jul 31 13:37:10 2014 - 000026 ms - RESULT -
% -----------------------------------------------------------------------------
%  linNet - The Software for symbolic Analysis of linear Electronic Circuits
%  Copyright (C) 1991, 2013-2014 Peter Vranken (mailto:Peter_Vranken@Yahoo.de)
%  This is free software; see the source for copying conditions. There is NO
%  warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
% -----------------------------------------------------------------------------
% Jul 31 13:37:10 2014 - 000026 ms - RESULT - Reading circuit file X:\linNet\trunk\components\linNet\doc\userGuide\sources\figures\bipolarTModel.cnl successfully done
% Jul 31 13:37:10 2014 - 000028 ms - RESULT - User-defined result DC:
% User-defined result DC:
% The solution for unknown U_C:
%   U_C(s) = N_U_C_Ubatt(s)/D_U_C_Ubatt(s) * Ubatt(s)
%            + N_U_C_Uin(s)/D_U_C_Uin(s) * Uin(s)
%            + N_U_C_Ube(s)/D_U_C_Ube(s) * Ube(s), with
%     N_U_C_Ubatt(s) = (R1*R2*Re*Rec*Ce*Cin*beta + R1*R2*Re*Rec*Ce*Cin) * s^2
%                      +(R1*R2*Re*Ce + R1*R2*Re*Cin*beta + R1*R2*Re*Cin + R1*R2*Rec*Ce
%                        + R1*Re*Rec*Ce*beta + R1*Re*Rec*Ce - R2*Rc*Re*Ce*beta
%                        - R2*Rc*Rec*Ce*beta + R2*Re*Rec*Ce*beta + R2*Re*Rec*Ce
%                       ) * s
%                      +(R1*R2 + R1*Re*beta + R1*Re - R2*Rc*beta + R2*Re*beta
%                        + R2*Re
%                       )
%     D_U_C_Ubatt(s) = (R1*R2*Re*Rec*Ce*Cin*beta + R1*R2*Re*Rec*Ce*Cin) * s^2
%                      +(R1*R2*Re*Ce + R1*R2*Re*Cin*beta + R1*R2*Re*Cin + R1*R2*Rec*Ce
%                        + R1*Re*Rec*Ce*beta + R1*Re*Rec*Ce + R2*Re*Rec*Ce*beta
%                        + R2*Re*Rec*Ce
%                       ) * s
%                      +(R1*R2 + R1*Re*beta + R1*Re + R2*Re*beta + R2*Re)
%     N_U_C_Uin(s) = -(R1*R2*Rc*Re*Ce*Cin*beta + R1*R2*Rc*Rec*Ce*Cin*beta) * s^2
%                    -R1*R2*Rc*Cin*beta * s
%     D_U_C_Uin(s) = D_U_C_Ubatt(s)
%     N_U_C_Ube(s) = (R1*R2*Rc*Re*Ce*Cin*beta + R1*R2*Rc*Rec*Ce*Cin*beta) * s^2
%                    +(R1*R2*Rc*Cin*beta + R1*Rc*Re*Ce*beta + R1*Rc*Rec*Ce*beta
%                      + R2*Rc*Re*Ce*beta + R2*Rc*Rec*Ce*beta
%                     ) * s
%                    +(R1*Rc*beta + R2*Rc*beta)
%     D_U_C_Ube(s) = D_U_C_Ubatt(s)
% The solution for unknown U_B:
%   U_B(s) = N_U_B_Ubatt(s)/D_U_B_Ubatt(s) * Ubatt(s)
%            + N_U_B_Uin(s)/D_U_B_Uin(s) * Uin(s)
%            + N_U_B_Ube(s)/D_U_B_Ube(s) * Ube(s), with
%     N_U_B_Ubatt(s) = (R2*Re*Rec*Ce*beta + R2*Re*Rec*Ce) * s
%                      +(R2*Re*beta + R2*Re)
%     D_U_B_Ubatt(s) = D_U_C_Ubatt(s)
%     N_U_B_Uin(s) = (R1*R2*Re*Rec*Ce*Cin*beta + R1*R2*Re*Rec*Ce*Cin) * s^2
%                    +(R1*R2*Re*Cin*beta + R1*R2*Re*Cin) * s
%     D_U_B_Uin(s) = D_U_C_Ubatt(s)
%     N_U_B_Ube(s) = (R1*R2*Re*Ce + R1*R2*Rec*Ce) * s
%                    +R1*R2
%     D_U_B_Ube(s) = D_U_C_Ubatt(s)
% The solution for unknown I_Ubatt:
%   I_Ubatt(s) = N_I_Ubatt_Ubatt(s)/D_I_Ubatt_Ubatt(s) * Ubatt(s)
%                + N_I_Ubatt_Uin(s)/D_I_Ubatt_Uin(s) * Uin(s)
%                + N_I_Ubatt_Ube(s)/D_I_Ubatt_Ube(s) * Ube(s), with
%     N_I_Ubatt_Ubatt(s) = (R2*Re*Rec*Ce*Cin*beta + R2*Re*Rec*Ce*Cin) * s^2
%                          +(R2*Re*Ce*beta + R2*Re*Ce + R2*Re*Cin*beta + R2*Re*Cin
%                            + R2*Rec*Ce*beta + R2*Rec*Ce + Re*Rec*Ce*beta
%                            + Re*Rec*Ce
%                           ) * s
%                          +(R2*beta + R2 + Re*beta + Re)
%     D_I_Ubatt_Ubatt(s) = D_U_C_Ubatt(s)
%     N_I_Ubatt_Uin(s) = (R1*R2*Re*Ce*Cin*beta + R1*R2*Rec*Ce*Cin*beta - R2*Re*Rec*Ce*Cin*beta
%                         - R2*Re*Rec*Ce*Cin
%                        ) * s^2
%                        +(R1*R2*Cin*beta - R2*Re*Cin*beta - R2*Re*Cin) * s
%     D_I_Ubatt_Uin(s) = D_U_C_Ubatt(s)
%     N_I_Ubatt_Ube(s) = -(R1*R2*Re*Ce*Cin*beta + R1*R2*Rec*Ce*Cin*beta) * s^2
%                        -(R1*R2*Cin*beta + R1*Re*Ce*beta + R1*Rec*Ce*beta
%                          + R2*Re*Ce*beta + R2*Re*Ce + R2*Rec*Ce*beta + R2*Rec*Ce
%                         ) * s
%                        -(R1*beta + R2*beta + R2)
%     D_I_Ubatt_Ube(s) = D_U_C_Ubatt(s)
% Jul 31 13:37:10 2014 - 000044 ms - RESULT - User-defined result G (Bode plot):
% User-defined result G (Bode plot):
% The dependency of U_out on Uin:
%   U_out(s) = N_U_out_Uin(s)/D_U_out_Uin(s) * Uin(s), with
%     N_U_out_Uin(s) = -(R1*R2*Rc*Re*Ce*Cin*beta + R1*R2*Rc*Rec*Ce*Cin*beta) * s^2
%                      -R1*R2*Rc*Cin*beta * s
%     D_U_out_Uin(s) = (R1*R2*Re*Rec*Ce*Cin*beta + R1*R2*Re*Rec*Ce*Cin) * s^2
%                      +(R1*R2*Re*Ce + R1*R2*Re*Cin*beta + R1*R2*Re*Cin + R1*R2*Rec*Ce
%                        + R1*Re*Rec*Ce*beta + R1*Re*Rec*Ce + R2*Re*Rec*Ce*beta
%                        + R2*Re*Rec*Ce
%                       ) * s
%                      +(R1*R2 + R1*Re*beta + R1*Re + R2*Re*beta + R2*Re)
% Jul 31 13:37:10 2014 - 000051 ms - RESULT - User-defined result Z (Bode plot):
% User-defined result Z (Bode plot):
% The dependency of Uin on I_Uin:
%   Uin(s) = N_Uin_I_Uin(s)/D_Uin_I_Uin(s) * I_Uin(s), with
%     N_Uin_I_Uin(s) = (R1*R2*Re*Rec*Ce*Cin*beta + R1*R2*Re*Rec*Ce*Cin) * s^2
%                      +(R1*R2*Re*Ce + R1*R2*Re*Cin*beta + R1*R2*Re*Cin + R1*R2*Rec*Ce
%                        + R1*Re*Rec*Ce*beta + R1*Re*Rec*Ce + R2*Re*Rec*Ce*beta
%                        + R2*Re*Rec*Ce
%                       ) * s
%                      +(R1*R2 + R1*Re*beta + R1*Re + R2*Re*beta + R2*Re)
%     D_Uin_I_Uin(s) = (R1*R2*Re*Ce*Cin + R1*R2*Rec*Ce*Cin + R1*Re*Rec*Ce*Cin*beta
%                       + R1*Re*Rec*Ce*Cin + R2*Re*Rec*Ce*Cin*beta + R2*Re*Rec*Ce*Cin
%                      ) * s^2
%                      +(R1*R2*Cin + R1*Re*Cin*beta + R1*Re*Cin + R2*Re*Cin*beta
%                        + R2*Re*Cin
%                       ) * s

% Operating point U_C:
Uin = 0
Ubatt = 12
Ube = 0.6
s = 0
R1 =  10000
R2 =  1200
Rc =  6800
Re =  680
Rec =  68
Ce = 4.7000e-006
Cin = 1.0000e-004
Cout = 1.5000e-006
beta = 250

     N_U_C_Ubatt = (R1*R2*Re*Rec*Ce*Cin*beta + R1*R2*Re*Rec*Ce*Cin) * s^2               ...
                      +(R1*R2*Re*Ce + R1*R2*Re*Cin*beta + R1*R2*Re*Cin + R1*R2*Rec*Ce   ...
                        + R1*Re*Rec*Ce*beta + R1*Re*Rec*Ce - R2*Rc*Re*Ce*beta           ...
                        - R2*Rc*Rec*Ce*beta + R2*Re*Rec*Ce*beta + R2*Re*Rec*Ce          ...
                       ) * s                                                            ...
                      +(R1*R2 + R1*Re*beta + R1*Re - R2*Rc*beta + R2*Re*beta            ...
                        + R2*Re                                                         ...
                       )
     D_U_C_Ubatt = (R1*R2*Re*Rec*Ce*Cin*beta + R1*R2*Re*Rec*Ce*Cin) * s^2               ...
                      +(R1*R2*Re*Ce + R1*R2*Re*Cin*beta + R1*R2*Re*Cin + R1*R2*Rec*Ce   ...
                        + R1*Re*Rec*Ce*beta + R1*Re*Rec*Ce + R2*Re*Rec*Ce*beta          ...
                        + R2*Re*Rec*Ce                                                  ...
                       ) * s                                                            ...
                      +(R1*R2 + R1*Re*beta + R1*Re + R2*Re*beta + R2*Re)
     N_U_C_Uin = -(R1*R2*Rc*Re*Ce*Cin*beta + R1*R2*Rc*Rec*Ce*Cin*beta) * s^2            ...
                    -R1*R2*Rc*Cin*beta * s
     D_U_C_Uin = D_U_C_Ubatt
     N_U_C_Ube = (R1*R2*Rc*Re*Ce*Cin*beta + R1*R2*Rc*Rec*Ce*Cin*beta) * s^2             ...
                    +(R1*R2*Rc*Cin*beta + R1*Rc*Re*Ce*beta + R1*Rc*Rec*Ce*beta          ...
                      + R2*Rc*Re*Ce*beta + R2*Rc*Rec*Ce*beta                            ...
                     ) * s                                                              ...
                    +(R1*Rc*beta + R2*Rc*beta)
     D_U_C_Ube = D_U_C_Ubatt

   U_C = N_U_C_Ubatt/D_U_C_Ubatt * Ubatt                                                ...
            + N_U_C_Uin/D_U_C_Uin * Uin                                                 ...
            + N_U_C_Ube/D_U_C_Ube * Ube

% Manual transformation of result:
D_2 = R1*R2 + (R1+R2)*Re*(1+beta)
U_C_2 = Ubatt + Rc*beta/D_2*(-R2*Ubatt + (R1+R2)*Ube) 