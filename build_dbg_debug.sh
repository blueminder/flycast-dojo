#!/bin/bash
rm debugger_cpp/*.so && rm debugger_cpp/.qmake.stash && cd debugger_cpp && qmake CONFIG+=debug debugger.pro && make clean && make -j4 && cp libdebugger.* ../build/


