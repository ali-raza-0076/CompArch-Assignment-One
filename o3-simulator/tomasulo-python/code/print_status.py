# Qing 
# 21st May 2017

# function: print RS/ROB status
def ps(unit):
    if(len(unit)==0):
        print('empty!')
    else:
        # get all fields in namedtuple
        fields_list = unit[0]._fields
        for unit_item in unit:
            print ('******')
            for field in fields_list:
                print(field+': ', str(getattr(unit_item, field)))

# function: print single namedtuple
def pss(snt):
    fields_list = snt._fields
    for field in fields_list:
        print (field+': ', str(getattr(snt, field)))

def print_registers(reg_int, reg_fp):
    print()
    print('REGISTER VALUES')
    print()
    print(''.join(('R'+str(i)).ljust(8) for i in range(len(reg_int))))
    print('value:')
    print(''.join(str(v).ljust(8) for v in reg_int))
    print()
    print(''.join(('F'+str(i)).ljust(8) for i in range(len(reg_fp))))
    print('value:')
    print(''.join(str(v).ljust(8) for v in reg_fp))

def print_memory(memory):
    print()
    print('NON-ZERO MEMORY VALUES')
    print()
    print('Addresses'.ljust(15)+'Values')
    for addr, value in enumerate(memory):
        if value != 0:
            print(str(addr).ljust(15)+str(value))

