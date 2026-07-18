"""
修改 DIYRuZRT.ewp 项目文件，启用 OTA 功能（仅 RouterEB 配置）：
1. RouterEB 配置的链接器从 f8w2530.xcl 切换到 ota.xcl
2. 添加 OTA 相关源文件到项目
"""
import re
import sys

EWP_PATH = r"d:\vc\diyruz_rt\DIYRuZRT\CC2530DB\DIYRuZRT.ewp"

with open(EWP_PATH, 'r', encoding='utf-8') as f:
    content = f.read()

# 找到 RouterEB 配置的范围
router_start = content.find('<configuration>\n        <name>RouterEB</name>')
if router_start == -1:
    print("ERROR: 找不到 RouterEB 配置")
    sys.exit(1)

# 找到 RouterEB 配置的结束 </configuration>
router_end = content.find('</configuration>', router_start)
if router_end == -1:
    print("ERROR: 找不到 RouterEB 配置结束")
    sys.exit(1)
router_end += len('</configuration>')

router_block = content[router_start:router_end]
print(f"RouterEB 配置范围: {router_start} ~ {router_end} (长度 {len(router_block)})")

# 1. 在 RouterEB 配置内替换 f8w2530.xcl 为 ota.xcl
old_xcl = r'$PROJ_DIR$\..\..\Tools\CC2530DB\f8w2530.xcl'
new_xcl = r'$PROJ_DIR$\..\..\Tools\CC2530DB\ota.xcl'

if old_xcl not in router_block:
    print("ERROR: RouterEB 配置中找不到 f8w2530.xcl")
    sys.exit(1)

new_router_block = router_block.replace(old_xcl, new_xcl, 1)
print("✓ RouterEB 链接器已切换: f8w2530.xcl → ota.xcl")

content = content[:router_start] + new_router_block + content[router_end:]

# 2. 添加 OTA 相关源文件（在 ZCL 组的 zcl_ha.h 后面添加）
# 文件列表是全局的（不在某个 configuration 内），所有配置共享
ota_files_zcl = """        <file>
            <name>$PROJ_DIR$\\..\\..\\Components\\stack\\zcl\\ota_common.c</name>
        </file>
        <file>
            <name>$PROJ_DIR$\\..\\..\\Components\\stack\\zcl\\ota_common.h</name>
        </file>
        <file>
            <name>$PROJ_DIR$\\..\\..\\Components\\stack\\zcl\\zcl_ota.c</name>
        </file>
        <file>
            <name>$PROJ_DIR$\\..\\..\\Components\\stack\\zcl\\zcl_ota.h</name>
        </file>
        <file>
            <name>$PROJ_DIR$\\..\\Source\\zcl_ha.h</name>
        </file>"""

# 在 zcl_ha.h 之后插入 OTA 文件
zcl_ha_h_anchor = """        <file>
            <name>$PROJ_DIR$\\..\\Source\\zcl_ha.h</name>
        </file>
    </group>"""

if zcl_ha_h_anchor not in content:
    print("ERROR: 找不到 zcl_ha.h 锚点")
    sys.exit(1)

# 替换：在 zcl_ha.h 的 </file> 后、</group> 前插入 OTA 文件
new_zcl_group = """        <file>
            <name>$PROJ_DIR$\\..\\Source\\zcl_ha.h</name>
        </file>
        <file>
            <name>$PROJ_DIR$\\..\\..\\Components\\stack\\zcl\\ota_common.c</name>
        </file>
        <file>
            <name>$PROJ_DIR$\\..\\..\\Components\\stack\\zcl\\ota_common.h</name>
        </file>
        <file>
            <name>$PROJ_DIR$\\..\\..\\Components\\stack\\zcl\\zcl_ota.c</name>
        </file>
        <file>
            <name>$PROJ_DIR$\\..\\..\\Components\\stack\\zcl\\zcl_ota.h</name>
        </file>
    </group>"""

content = content.replace(zcl_ha_h_anchor, new_zcl_group, 1)
print("✓ 已添加 OTA ZCL 源文件: ota_common.c/h, zcl_ota.c/h")

# 3. 在 HAL Drivers 组（CC2530EB）添加 hal_ota.c/h
# 找到 hal_led.c 之后的位置
hal_led_anchor = """                    <file>
                        <name>$PROJ_DIR$\\..\\..\\Components\\hal\\target\\CC2530EB\\hal_led.c</name>
                    </file>"""

hal_ota_files = """                    <file>
                        <name>$PROJ_DIR$\\..\\..\\Components\\hal\\target\\CC2530EB\\hal_led.c</name>
                    </file>
                    <file>
                        <name>$PROJ_DIR$\\..\\..\\Components\\hal\\target\\CC2530EB\\hal_ota.c</name>
                    </file>
                    <file>
                        <name>$PROJ_DIR$\\..\\..\\Components\\hal\\target\\CC2530EB\\hal_ota.h</name>
                    </file>"""

if hal_led_anchor not in content:
    print("ERROR: 找不到 hal_led.c 锚点")
    sys.exit(1)

content = content.replace(hal_led_anchor, hal_ota_files, 1)
print("✓ 已添加 HAL OTA 源文件: hal_ota.c/h")

# 4. 在 MT 组添加 MT_OTA.c/h（如果存在）
mt_ota_path = r'$PROJ_DIR$\..\..\Components\mt\MT_OTA.c'
if mt_ota_path not in content:
    # 找到 MT 组的某个文件作为锚点
    mt_anchor = """        <file>
            <name>$PROJ_DIR$\\..\\..\\Components\\mt\\MT_SYS.c</name>
        </file>"""

    mt_ota_insert = """        <file>
            <name>$PROJ_DIR$\\..\\..\\Components\\mt\\MT_SYS.c</name>
        </file>
        <file>
            <name>$PROJ_DIR$\\..\\..\\Components\\mt\\MT_OTA.c</name>
        </file>
        <file>
            <name>$PROJ_DIR$\\..\\..\\Components\\mt\\MT_OTA.h</name>
        </file>"""

    if mt_anchor in content:
        content = content.replace(mt_anchor, mt_ota_insert, 1)
        print("✓ 已添加 MT OTA 源文件: MT_OTA.c/h")
    else:
        print("⚠ 找不到 MT_SYS.c 锚点，跳过 MT_OTA 添加")
else:
    print("✓ MT_OTA.c 已存在于项目中")

# 写回文件
with open(EWP_PATH, 'w', encoding='utf-8') as f:
    f.write(content)

print(f"\n✓ DIYRuZRT.ewp 修改完成: {EWP_PATH}")
