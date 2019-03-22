def check_file_header(lines, f_header='example_file_header.txt'):
    """
    check if the file has a correct header and add one if check fail
    :param lines: the file content
    :param f_header: the file that contains the right header
    """
    f = True
    with open(f_header, 'r') as fin:
        header = fin.readlines()
        if len(header) > len(lines):
            f = False
        for idx in range(len(header)):
            if header[idx] != lines[idx]:
                f = False
                break
        if not f:
            lines[0:0] = header


def check_file_annotation(lines, idx):
    f_file = False
    f_brief = False
    while lines[idx].strip().startswith('//!'):
        if lines[idx].strip().startswith('//! \\file'):
            f_file = True
        if lines[idx].strip().startswith('//! \\brief'):
            f_brief = True
        idx += 1
    if f_file and f_brief:
        return True, idx
    else:
        return False, idx


def add_file_annotation(lines, filename):
    lines.append('//!\n')
    lines.append('//! \\file     ' + filename + ' \n')
    lines.append('//! \\brief\n')
    lines.append('//!\n')


def get_method_info(s):
    method_info = {
        'return_type': '',
        'method_name': '',
        'method_test_name': '',
        'parameters': [],
        'virtual': False
    }

    if s.startswith('virtual'):
        method_info['virtual'] = True
        s = s[8:].strip()

    # if s.startswith('void'):
    #     method_info['return_type'] = 'void'
    # elif s.startswith('MOS_STATUS'):
    #     method_info['return_type'] = 'MOS_STATUS'
    # print(s)
    idx0 = s.find(' ')
    method_info['return_type'] = s[:idx0].strip()
    if method_info['return_type'].find('(') is not -1:
        method_info['return_type'] = ''
    idx1 = s.find('(')
    idx2 = s.find(')')
    method_info['method_name'] = s[idx0+1:idx1].strip()
    s_para = s[idx1+1:idx2]
    paras = s_para.split(',')
    for i in paras:
        tmp = i.strip().split(' ')
        if len(tmp) == 2:
            if tmp[0][-1] == '&':
                tmp[0] = tmp[0][:-1]
                tmp[1] = '&' + tmp[1]
            para = {'type': tmp[0].strip(), 'name': tmp[1].strip()}
            method_info['parameters'].append(para)
    method_info['method_test_name'] = method_info['method_name'] + 'Test'
    # print(method_info)
    return method_info


def get_method_declaration(lines, idx):
    method = lines[idx].strip()
    while method.find(';') == -1:
        idx += 1
        method += lines[idx].strip()
    return method


def check_method_annotation(lines, idx):
    f = False
    f_brief = False
    f_para = False
    f_return = False
    idx -= 1
    while lines[idx].strip() == '' or lines[idx].strip().startswith('//!'):
        s = lines[idx].strip()
        if s.startswith('//! \\brief'):
            f_brief = True
        elif s.startswith('//! \\return'):
            f_return = True
        elif s.startswith('//! \\para'):
            f_para = True
        idx -= 1

    f = f_brief or f_para or f_return
    return f


def add_method_annotation(lines, m, idt):
    lines.append('\n')
    lines.append(idt + '//!\n')
    #lines.append(idt + '//! \\brief\n')
    if m['method_name'].startswith('~'):
        lines.append(idt + '//! \\brief  Destructor of class ' + m['method_name'][1:] + '\n')
    elif m['return_type'] == '':
        lines.append(idt + '//! \\brief  Constructor of class ' + m['method_name'][1:] + '\n')
    else:
        lines.append(idt + '//! \\brief\n')

    if m['parameters']:
        for p in m['parameters']:
            lines.append(idt + '//! \\param  [in]' + p['name'] + '\n')
            lines.append(idt + '//!\n')
    if m['return_type']:
        lines.append(idt + '//! \\return ' + m['return_type'] + '\n')
        if m['return_type'] == 'MOS_STATUS':
            lines.append(idt + '//!         MOS_STATUS_SUCCESS if success, else fail reason\n')
        else:
            lines.append(idt + '//!\n')
    lines.append(idt + '//!\n')


def get_indentation(line):
    s = line.strip()
    idx = line.find(s)
    return line[:idx]


def check_annotation(lines, file_name):
    """

    :param lines: a list contains source code
    :param file_name:
    :return: a list contains the codes with annotation
    """
    newlines = []

    f_file_anno = False
    for idx in range(len(lines)):
        s = lines[idx].strip()
        if s == '' or s.startswith('/*') or s.startswith('*'):
            continue
        if s == '//!':
            f_file_anno, p = check_file_annotation(lines, idx)
        else:
            p = idx
        break

    newlines.extend(lines[:p])
    if not f_file_anno:
        add_file_annotation(newlines, file_name)

    if file_name.endswith('.cpp'):
        newlines.extend(lines[p:])
        return newlines

    f = False
    for idx in range(p, len(lines)):
        s = lines[idx].strip()
        idx2 = s.find('//')
        if idx2 is not -1 and s.find('//!') is -1:
            s = s[:idx2]
        if s.find('(') is not -1:
            if f:
                f = False
                newlines.append(lines[idx])
                continue
            dec = get_method_declaration(lines, idx)
            m = get_method_info(dec)
            f_method_anno = check_method_annotation(lines, idx)
            if not f_method_anno:
                indentation = get_indentation(lines[idx])
                add_method_annotation(newlines, m, indentation)
            if s.find(':') is not -1:
                f = True
        newlines.append(lines[idx])

    return newlines


def main():
    file_list = []
    with open('file_list.txt', 'r') as fin:
        for line in fin:
            file_list.append(line.strip())

    for file_name in file_list:
        with open(file_name, 'r') as fin:
            lines = fin.readlines()
        newlines = check_annotation(lines, file_name)
        check_file_header(newlines)
        idx = file_name.rfind('/')
        with open(file_name[:idx+1] + 'test_' + file_name[idx+1:], 'w') as fout:
            fout.writelines(newlines)


if __name__ == '__main__':
    main()
