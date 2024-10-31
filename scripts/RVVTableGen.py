import sys
import argparse
import os
import re


def operandType(category):
    opType = 'RegUNKNOWN'
    if category == 0x00:
        opType = 'RegVEC'
    elif category == 0x01:
        opType = 'RegVEC'
    elif category == 0x02:
        opType = 'RegVEC'
    elif category == 0x03:
        opType = 'RegIMM'
    elif category == 0x04:
        opType = 'RegGPR'
    elif category == 0x05:
        opType = 'RegFLOAT'
    elif category == 0x06:
        opType = 'RegGPR'
    elif category == 0x07:
        opType = 'RegGPR'

    return opType


parser = argparse.ArgumentParser()
parser.add_argument('opcode_filename', type=str, help='File containing list of opcodes from riscv-opcodes github repo')

args = parser.parse_args()
if not os.path.isfile(args.opcode_filename):
    sys.exit('File ' + args.opcode_filename + ' not found')

opcodeFile = open(args.opcode_filename, 'r')

# Iterate over all opcodes
instructions = []
for line in opcodeFile:
    # ignore comments
    if '#' in line:
        continue

    # split on whitespace
    fields = line.split()

    # if there are not enough fields we dont have a line with an instruction
    if len(fields) < 3:
        continue

    opcode = fields[0]

    inst = []
    bits = ''
    rdClass = ''
    rs1Class = ''
    rs2Class = ''
    rs3Class = ''
    width = 0
    lumop = 0
    funcPtr = ''
    operandFormat = 'cfg'
    if "vset" not in opcode:
        operandFormat = opcode.split('.')[1]

    # grab all fields in the instruction - ignore the 1st as that is the opcode
    for field in fields[1:]:
        if '6..0=' in field:
            opcode_hex = int(field.split('=')[1], 16)
            bits = bin(int(field.split('=')[1], 16))
        elif '=' in field:
            continue
        else:
            inst.append(field)

    # ### Create mnemonic ####
    # instruction fields are in the opposite order we want them for a mnemonic, so reverse
    inst.reverse()
    # insert % after the fields
    inst = ["%" + str(m) for m in inst]

    # convert list of fields to single comma delineated string
    mnemonic = str(opcode) + " " + ", ".join(str(elem) for elem in inst)
    instructions.append(mnemonic)

    if 'rd' in inst:
        rdClass = 'RegGPR'
    elif 'vd' in inst[0]:
        rdClass = 'RegVEC'

    if 'rs1' in inst:
        rs1Class = 'RegGPR'
    elif 'vs1' in inst:
        rs1Class = 'RegVEC'
    else:
        rs1Class = 'RegUNKNOWN'

    if 'rs2' in inst:
        rs2Class = 'RegGPR'
    elif 'vs2' in inst:
        rs2Class = 'RegVEC'
    else:
        rs2Class = 'RegUNKNOWN'

    if 'rs3' in inst:
        rs3Class = 'RegGPR'
    elif 'vs3' in inst:
        rs3Class = 'RegVEC'
    else:
        rs3Class = 'RegUNKNOWN'

    # Check for L/S opcode
    if (opcode_hex == 0x07) or (opcode_hex == 0x27):
        w = re.compile('14..12.*')
        width = int(list(filter(w.match, fields))[0].split('=')[1], 16)

        l = re.compile('24..20.*')
        m = list(filter(l.match, fields))
        if len(m) > 0:
            lumop = int(m[0].split('=')[1],16)
        else:
            lumop = 0

        mop_x = re.compile('27..26.*')
        mop = int(list(filter(mop_x.match, fields))[0].split('=')[1],16)
        if mop == 0:
            if lumop == 0x10:
                funcPtr = 'vlsunitStrideFirst'
            elif lumop == 0x08:
                funcPtr = 'vlsWhole'
            else:
                funcPtr = 'vlsunitStride'
        elif mop == 1:
            funcPtr = 'vlsIndexUnorder'
        elif mop == 2:
            funcPtr = 'vlsStride'
        elif mop == 3:
            funcPtr = 'vlsIndexOrder'

    # Check for ALU opcode
    elif (opcode_hex == 0x57 and ('vset' not in opcode)):
        ar = re.compile('31..26.*')

        l_t = list(filter(ar.match, fields))

        if len(l_t) > 0:

               # all vector ALU ops have vector regfile as the 2nd operand type
               rs2Class = 'RegVEC'

               alu_op_txt = l_t[0].split('=')
               alu_op = int(alu_op_txt[1],16)

               w = re.compile('14..12.*')
               minor_op = int(list(filter(w.match, fields))[0].split('=')[1], 16)

               # operand 1 is depending on the vector operation type (vector-vector op, vector-scalar-op, etc.)
               rs1Class = operandType(minor_op)

               if  (alu_op == 0x00) or (alu_op == 0x35):
                   funcPtr = 'vadd'
               elif alu_op == 0x01:
                   funcPtr = 'vredsum'
               elif (alu_op == 0x02) or ((alu_op == 0x23) and (minor_op == 0x00 or minor_op == 0x04)) or (alu_op == 0x36) or (alu_op == 0x12 and (minor_op == 0x04 or minor_op == 0x00)):
                   funcPtr = 'vsub'
               elif alu_op == 0x03:
                   funcPtr = 'vredosum'
               elif alu_op == 0x04:
                   funcPtr = 'vmin'
               elif alu_op == 0x05:
                   funcPtr = 'vredmin'
               elif alu_op == 0x06:
                   funcPtr = 'vmax'
               elif alu_op == 0x07:
                   funcPtr = 'vredmax'
               elif alu_op == 0x08:
                   funcPtr = 'vsgnj'
               elif alu_op == 0x09:
                   funcPtr = 'vsgnjn'
               elif alu_op == 0x0a:
                   funcPtr = 'vsgnjx'
               elif alu_op == 0x0b:
                   funcPtr = 'vxor'
               elif alu_op == 0x0c:
                   funcPtr = 'vgather'
               elif alu_op == 0xe:
                   funcPtr = 'vslide1up'
               elif alu_op == 0xf:
                   funcPtr = 'vslide1down'
               elif alu_op == 0x10:
                   funcPtr = 'vmv_merge'
               elif alu_op == 0x11:
                   funcPtr = 'vmadc'
               elif alu_op == 0x12 and minor_op == 0x01:
                   funcPtr = 'vtype_conv'
               elif alu_op == 0x12 and minor_op == 0x02:
                   funcPtr = 'vzext'
               elif alu_op == 0x13 and minor_op == 0x01:
                   funcPtr = 'vextended_math'
               elif alu_op == 0x14:
                   funcPtr = 'vmask_index'
               elif alu_op == 0x17:
                   funcPtr = 'vmv_merge'
               elif alu_op == 0x18:
                   funcPtr = 'veq'
               elif alu_op == 0x19:
                   funcPtr = 'vle'
               elif alu_op == 0x1a:
                   funcPtr = 'vltu'
               elif alu_op == 0x1b:
                   funcPtr = 'vlt'
               elif alu_op == 0x1c:
                   funcPtr = 'vne'
               elif alu_op == 0x1d:
                   funcPtr = 'vgt'
               elif alu_op == 0x1e:
                   funcPtr = 'vgtu'
               elif alu_op == 0x1f:
                   funcPtr = 'vge'

                   #mismatch
               elif alu_op == 0x20:
                   funcPtr = 'vdiv'
               elif alu_op == 0x21:
                   funcPtr = 'vrdiv'
               elif alu_op == 0x22:
                   funcPtr = 'vrdiv'
               elif alu_op == 0x23 and (minor_op == 0x02 or minor_op == 0x06):
                   funcPtr = 'vrem'
               elif (alu_op == 0x24) or (alu_op == 0x25 and (minor_op == 0x02 or minor_op == 0x06)) or (alu_op == 0x26) or (alu_op == 0x3a):
                   funcPtr = 'vmul'
               elif (alu_op == 0x25) and (minor_op == 0x04 or minor_op ==0x00) or (alu_op == 0x3b):
                   funcPtr = 'vsll'
               elif alu_op == 0x27:
                   funcPtr = 'vrsub'
               elif alu_op == 0x28:
                   funcPtr = 'vmadd'
               elif alu_op == 0x29:
                   funcPtr = 'vnmadd'
               elif alu_op == 0x2a or (alu_op == 0x13 and (minor_op == 0x04 or minor_op == 0x00)):
                   funcPtr = 'vmsub'
               elif alu_op == 0x2b:
                   funcPtr = 'vnmsub'
               elif alu_op == 0x2c:
                   funcPtr = 'vmacc'
               elif alu_op == 0x2d:
                   funcPtr = 'vnmacc'
               elif alu_op == 0x2e:
                   funcPtr = 'vmsac'
               elif alu_op == 0x2f:
                   funcPtr = 'vnmsac'
               elif alu_op == 0x30:
                   funcPtr = 'vwadd'
               elif alu_op == 0x31:
                   funcPtr = 'vwredsum'
               elif alu_op == 0x32 or alu_op == 0x37:
                   funcPtr = 'vwsub'
               elif alu_op == 0x33:
                   funcPtr = 'vwredosum'
               elif alu_op == 0x34:
                   funcPtr = 'vwaddw'
               elif alu_op == 0x36:
                   funcPtr = 'vwsubw'
               elif alu_op == 0x38:
                   funcPtr = 'vwmul'
               elif alu_op == 0x39:
                   funcPtr = 'vdot'
               elif alu_op == 0x3c:
                   funcPtr = 'vwmacc'
               elif alu_op == 0x3d:
                   funcPtr = 'vwnmacc'
               elif alu_op == 0x3e:
                   funcPtr = 'vwmsac'
               elif alu_op == 0x3f:
                   funcPtr = 'vwnmsac'

    elif ((opcode_hex == 0x57) and ('vset' in opcode)):
        rdClass = 'RegGPR'
        width = 0x07
        msbs = re.compile('31')
        msb = int(list(filter(msbs.match, fields))[0].split('=')[1], 16)

        msbs2 = re.compile('30..25')
        vl = list(filter(msbs2.match, fields))

        if( (msb == 0x01) and ( len(vl) > 0)):
            funcPtr = 'vsetvl'
        elif( msb == 0x00):
            funcPtr = 'vsetvli'
        else:
            funcPtr = 'vsetivli'


    else:
        width = 0
        lumop = 0
        continue


    tableEntry = ("{RevInstEntryBuilder<RevVecInstDefaults>().SetMnemonic(\"" + mnemonic + "\").SetCost(1).SetOpcode(" + bits + ")"
                  ".SetrdClass(" + rdClass + ").Setrs1Class(" + rs1Class + ").Setrs2Class(" + rs2Class+ ").Setrs3Class(" + rs3Class + ")"
                  ".SetFunct3(" + hex(width) + ").SetImplFunc(&" + funcPtr + ").SetOperandFormat(" + operandFormat + ").SetFormat(RVTypeV).InstEntry},")
    print(tableEntry)

