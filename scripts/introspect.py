import sys,inspect,importlib
sys.path.insert(0,'/usr/share/klipper/klippy'); sys.path.insert(0,'/usr/share/klipper/klippy/extras')
for m in sys.argv[1:]:
    print('#####',m)
    try: mod=importlib.import_module('extras.'+m)
    except Exception as e: print('ERRO',repr(e)); continue
    for n,o in vars(mod).items():
        if inspect.isclass(o) and getattr(o,'__module__','')==mod.__name__:
            ms=[k for k,v in vars(o).items() if callable(v)]
            print('class',n,'(',len(ms),'métodos):',' '.join(ms))
        elif callable(o) and getattr(o,'__module__','')==mod.__name__: print('func',n)
