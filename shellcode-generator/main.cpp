#include "coff.h"

#include "rang_impl.hpp"
#include <fstream>
#include <functional>
#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <regex>
#include <intrin.h>
#include <iomanip>
#define ZH_CN


void open_binary_file(const std::string& file, std::vector<uint8_t>& data)
{
	std::ifstream fstr(file, std::ios::binary);
	fstr.unsetf(std::ios::skipws);
	fstr.seekg(0, std::ios::end);

	const auto file_size = fstr.tellg();

	fstr.seekg(NULL, std::ios::beg);
	data.reserve(static_cast<uint32_t>(file_size));
	data.insert(data.begin(), std::istream_iterator<uint8_t>(fstr), std::istream_iterator<uint8_t>());
}

void buffer_to_file_bin(unsigned char *buffer, size_t buffer_size, const std::string &filename) {
    std::ofstream file(filename, std::ios_base::out | std::ios_base::binary);
    file.write((const char *)buffer, buffer_size);
    file.close();
}



std::string &replace_all(std::string &str, const std::string &old_value, const std::string &new_value) {
    while (true) {
        std::string::size_type pos(0);
        if ((pos = str.find(old_value)) != std::string::npos)
            str.replace(pos, old_value.length(), new_value);
        else
            break;
    }
    return str;
}


struct relocation_info {
    std::string sym_name;
    uint32_t    va;
    uint32_t    type;
};
struct prepare_mapped_symbols {
    //before maped
    std::string sym_name;
    uint8_t *   bytes;
    uint32_t    bytes_size;
    //after maped
    uint32_t maped_va;
    uint32_t maped_size;

    std::vector<relocation_info> relocations;
};

// �ݹ���Һ������������鿴ÿ���������ض�λ��Ϣ�������ض�λ��Ϣ���ض�λ��Ϣ���ң�ѭ������
// involves ������γɵ�����Ӧ�ñ����ӵ� ����
void recursive_lookup_relocations(std::set<std::string> &system_api,
                                  std::set<std::string> &not_found,
                                  std::map<std::string, prepare_mapped_symbols> &involves,
                                  std::vector<coff::lib> &libs,
                                  coff::obj &             obj,
                                  IMAGE_SYMBOL &          symbol) {
    if (symbol.SectionNumber == IMAGE_SYM_UNDEFINED) {

        std::string symbol_name = obj.symbol_name(symbol);
        for (auto &lib : libs) {
            for (auto &other_obj : lib.objs()) {
                auto new_symbol = other_obj.get_symbol(symbol_name);
                if (new_symbol /*&& other_obj.name() != obj.name()*/) {
                    if (new_symbol->SectionNumber > IMAGE_SYM_UNDEFINED)
                        recursive_lookup_relocations(system_api, not_found, involves, libs, other_obj, *new_symbol);
                }
            }
        }

        if (involves.find(symbol_name) == involves.end()) {
            if (_ReturnAddress() != (void *)&recursive_lookup_relocations) {

                //  ����Ĵ�����Ϊ������: ��shellcode�����Զ����api�ĵ�����ö�д�ģ�����������ȥʹ��lazy_importer
                std::smatch base_match;
                if (std::regex_match(symbol_name, base_match, std::regex("^__imp_(.*?)"))) {
                    system_api.insert(symbol_name);
#ifdef ZH_CN
                    IMP("\tϵͳAPI: \"%s\"", base_match[1].str().c_str());
#else
                    IMP("\tWindows System API: \"%s\"", base_match[1].str().c_str());
#endif // ZH_CN
                } else {
                    not_found.insert(symbol_name);
#ifdef ZH_CN
                    ERO("\t\t�޷��ҵ��ķ��� \"%s\" >> ����һ����Ч�ķ��ţ���δ���κ�.cpp�ļ���ʵ�ָ÷��ŵĶ���",
                        symbol_name.c_str());
#else
                    ERO("\t\tUnable to find symbol named \"%s\" >> This is an invalid symbol and you are not in any The "
                        "definition "
                        "of the symbol is implemented in the .cpp file",
                        symbol_name.c_str());
#endif // ZH_CN
                    
                }
            }
        }
    }

    // Ҫȷ��������Ч����ǰobj�ڣ�
    if (symbol.SectionNumber > IMAGE_SYM_UNDEFINED) {
        
        //�����ڱ�obj��
        IMAGE_SECTION_HEADER &section = obj.sections()[static_cast<size_t>(symbol.SectionNumber) - 1];

        prepare_mapped_symbols pre_sym;
        pre_sym.sym_name = obj.symbol_name(symbol);
        pre_sym.bytes    = static_cast<size_t>(section.PointerToRawData) + obj.obj_data();
        pre_sym.bytes_size = static_cast<uint32_t>(section.SizeOfRawData);
        involves.insert({pre_sym.sym_name, pre_sym});

        // �ض�λ��Ϣ
        auto &relocations = obj.relocations(&section);
        for (auto &reloc : relocations) {

            // �ض�λ����
            auto &      reloc_symbol = obj.symbols()[reloc.SymbolTableIndex];
            std::string symbol_name  = obj.symbol_name(reloc_symbol);

            relocation_info reloc_info;
            reloc_info.va = reloc.VirtualAddress;
            reloc_info.sym_name = symbol_name;
            reloc_info.type     = reloc.Type;
            involves[pre_sym.sym_name].relocations.push_back(reloc_info);

            if (involves.find(symbol_name) == involves.end()) {
                //INF("%s       %s", obj.name().c_str(), symbol_name.c_str());
                recursive_lookup_relocations(system_api, not_found,involves, libs, obj, reloc_symbol);
            }
        }
    }
}


void print_exports(std::vector<coff::lib> &libs) {
    for (auto &lib : libs) {
        auto &objs = lib.objs();
        // ���� obj �ļ��鿴����
        for (auto &obj : objs) {
            for (auto &exp : obj.exports()) {
#ifdef ZH_CN
                INF("\"%s\"�о��е�������:\"%s\"", obj.name().c_str(), exp.c_str());
#else
                INF("export symbol:\"%s\" in \"%s\"", exp.c_str(), obj.name().c_str());
#endif // ZH_CN
            }
        }
    }
}
std::vector<std::string> get_exports(std::vector<coff::lib> &libs) {
    std::vector<std::string> ret;
    for (auto &lib : libs) {
        auto &objs = lib.objs();
        for (auto &obj : objs) {
            for (auto &exp : obj.exports()) {
                ret.push_back(exp);
            }
        }
    }
    return ret;
}

void                     print_shellcode_hpp_file(std::string                                    resource_name,
                              std::vector<std::string>                      exports,
                              std::map<std::string, prepare_mapped_symbols> &involves,
                              std::vector<uint8_t> &                         shellcodebytes) {
    //������ļ�
    std::ofstream outFile;
    outFile.open(resource_name + ".hpp", std::ios::out);

    //���ͷ����Ϣ
    outFile << "#pragma once" << std::endl;
    outFile << "#include <cstdint>" << std::endl;
    outFile << "namespace shellcode\n{" << std::endl;

    outFile << "namespace rva\n{" << std::endl;
    for (auto &exp : exports) {




#ifdef _M_IX86 // 32λģʽ�� ���������ں���ǰ���һ�� _
        uint32_t maped_va = involves[exp].maped_va;
        if (exp.front() == '_') {
            exp.erase(exp.begin());
        }
        outFile << "const size_t " << exp << " = 0x" << std::hex << maped_va << ";\n";
#else
        outFile << "const size_t " << exp << " = 0x" << std::hex << involves[exp].maped_va << ";\n";
#endif // _M_IX86

        
    }
    outFile << "\n}\n" << std::endl;

    outFile << "const unsigned char " + resource_name + " [] = " << std::endl;
    outFile << "\t{" << std::endl << "\t";
    
    for (size_t idx = 0; idx < shellcodebytes.size(); idx++) {
        if (idx % 80 == 0)
            outFile << "\n"; 
        uint8_t code_byte = shellcodebytes[idx];
        outFile << "0x" << std::hex << std::setw(2) << std::setfill('0') << (int)code_byte << ",";
    }





    outFile << "\t};" << std::endl;

    outFile << "\n};\n" << std::endl;
    outFile.close();
}


int main() {

    // δ����֧��lib��������������������ܻ�������lib
    std::vector<coff::lib> libs;

    std::vector<uint8_t> data;
    open_binary_file("shellcode-payload.lib", data);
    coff::lib payload(data.data(), data.size());

    libs.push_back(payload);

    // �����漰���ķ�����
    std::map<std::string, prepare_mapped_symbols> involves;
    std::set<std::string> system_api;
    std::set<std::string> not_found;

    //��ӡ���еĵ�������
    print_exports(libs);

    // ���� �ҳ�: �����������������š�ϵͳapi�� δ����ķ���
    for (auto &lib : libs) {
        auto &objs = lib.objs();
        //���� obj Ϊ��������Ѱ������������
        for (auto &obj : objs) {
            for (auto &exp : obj.exports()) {
                obj.for_each_symbols([&](IMAGE_SYMBOL &symbol) {
                    const char *symbol_name = obj.symbol_name(symbol);
                    if (symbol_name == exp) {
                        //involves.insert(symbol_name);
#ifdef ZH_CN
                        INF("Ϊ��������\"%s\"������������", exp.c_str());
#else
                        INF("Find link dependency for export symbol:\"%s\"", exp.c_str());
#endif // ZH_CN
                        recursive_lookup_relocations(system_api, not_found, involves, libs, obj, symbol);
                    }
                });
            }
        }
    }


    
    if (!not_found.empty()) {
#ifdef ZH_CN
        ERO("�����޷��ҵ����ⲿ����,����ֹͣ");
#else
        INF("There is external symbols that cannot be found. The link is stopped");
#endif // ZH_CN
        std::system("pause");
        return 0;
    }


    // ����������Ч���ŵ��ܴ�С��ÿ������ֱ�Ӷ������ 4 �ֽڶ���
    uint32_t shell_size = 0;
    for (auto &i : involves) {
        shell_size += i.second.bytes_size;
        shell_size += i.second.bytes_size % sizeof(uint32_t);
    }

    std::vector<uint8_t> shellcodebytes(static_cast<size_t>(shell_size), 0xcc );


    size_t   va     = 0;
    uint8_t *cursor = shellcodebytes.data();
    for (auto &symbol_info : involves) {
        memcpy(cursor, symbol_info.second.bytes, static_cast<size_t>(symbol_info.second.bytes_size));
        symbol_info.second.maped_va = va;
        symbol_info.second.maped_size =
            symbol_info.second.bytes_size + (symbol_info.second.bytes_size % sizeof(uint32_t));
        
        INF("����\"%s\"������:\n\t\trva:0x%x/size:0x%x",
            symbol_info.first.c_str(),
            symbol_info.second.maped_va,
            symbol_info.second.maped_size);

        va += symbol_info.second.maped_size;
        cursor += symbol_info.second.maped_size; 
    }

    for (auto &symbol_info : involves) {
        for (auto &reloca : symbol_info.second.relocations) {
#ifdef _WIN64
            if (reloca.type == IMAGE_REL_AMD64_REL32) {
                *reinterpret_cast<int *>(static_cast<size_t>(reloca.va) + shellcodebytes.data() + symbol_info.second.maped_va) =
                    static_cast<int>(involves[reloca.sym_name].maped_va -
                                     (symbol_info.second.maped_va + reloca.va + sizeof(uint32_t))
                    );
            } else if (reloca.type == IMAGE_REL_AMD64_REL32_1) {
                *reinterpret_cast<int *>(static_cast<size_t>(reloca.va) + shellcodebytes.data() +
                                         symbol_info.second.maped_va) =
                    static_cast<int>(involves[reloca.sym_name].maped_va -
                                     (1 + symbol_info.second.maped_va + reloca.va + sizeof(uint32_t)));
            } else if (reloca.type == IMAGE_REL_AMD64_REL32_2) {
                *reinterpret_cast<int *>(static_cast<size_t>(reloca.va) + shellcodebytes.data() +
                                         symbol_info.second.maped_va) =
                    static_cast<int>(involves[reloca.sym_name].maped_va -
                                     (2 + symbol_info.second.maped_va + reloca.va + sizeof(uint32_t)));
            } else if (reloca.type == IMAGE_REL_AMD64_REL32_3) {
                *reinterpret_cast<int *>(static_cast<size_t>(reloca.va) + shellcodebytes.data() +
                                         symbol_info.second.maped_va) =
                    static_cast<int>(involves[reloca.sym_name].maped_va -
                                     (3 + symbol_info.second.maped_va + reloca.va + sizeof(uint32_t)));
            } else if (reloca.type == IMAGE_REL_AMD64_REL32_4) {
                *reinterpret_cast<int *>(static_cast<size_t>(reloca.va) + shellcodebytes.data() +
                                         symbol_info.second.maped_va) =
                    static_cast<int>(involves[reloca.sym_name].maped_va -
                                     (4 + symbol_info.second.maped_va + reloca.va + sizeof(uint32_t)));
            } else if (reloca.type == IMAGE_REL_AMD64_REL32_5) {
                *reinterpret_cast<int *>(static_cast<size_t>(reloca.va) + shellcodebytes.data() +
                                         symbol_info.second.maped_va) =
                    static_cast<int>(involves[reloca.sym_name].maped_va -
                                     (5 + symbol_info.second.maped_va + reloca.va + sizeof(uint32_t)));
            
            } 
#else
            if (reloca.type == IMAGE_REL_I386_REL32) {
                *reinterpret_cast<int *>(static_cast<size_t>(reloca.va) + shellcodebytes.data() +
                                         symbol_info.second.maped_va) =
                    static_cast<int>(involves[reloca.sym_name].maped_va -
                                     (symbol_info.second.maped_va + reloca.va + sizeof(uint32_t)));
            } 
#endif // _WIN64
            else {
#ifdef ZH_CN
                if (reloca.type == IMAGE_REL_I386_DIR32) {
                    ERO("��ǰ��δ��� IMAGE_REL_I386_DIR32 �ض�λ����");
                }
                ERO("�����޷������CPU�ض�λģʽ [0x%x] ��鿴 "
                    "pecoff(microsoft����ֲ��ִ���ļ���ͨ��Ŀ���ļ���ʽ�ļ��淶).pdf: 5.2.1 ����ָʾ�� "
                    "���޸�������,����ֹͣ",
                    reloca.type);
#else
                INF("There are CPU relocation modes that cannot be processed. The link is stopped");
#endif // ZH_CN
                std::system("pause");
                return 0;
            }
        }
    }

    buffer_to_file_bin(shellcodebytes.data(), shellcodebytes.size(), "shellcode-payload.bin");
    print_shellcode_hpp_file("payload", get_exports(libs), involves, shellcodebytes);
    
    INF("shellcode ���ɳɹ�");

    std::system("pause");

    return 0;
}

