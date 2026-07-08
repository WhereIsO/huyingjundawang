from __future__ import annotations

import html
import zipfile
from pathlib import Path


OUT = Path("docs/project_presentation.pptx")

SLIDES = [
    (
        "项目目标",
        [
            "实现一款基本可用的数据备份软件",
            "支持目录树备份和数据还原",
            "覆盖打包、压缩、加密、元数据、筛选、图形界面等扩展功能",
            "提供构建脚本、源码和实验文档",
        ],
    ),
    (
        "需求分析",
        [
            "基础功能：将源目录保存到指定归档文件",
            "基础功能：从归档恢复到指定目标目录",
            "可用性：命令行参数清晰，错误信息明确",
            "健壮性：处理错误密码、损坏归档、路径逃逸和覆盖冲突",
        ],
    ),
    (
        "功能实现",
        [
            "backup：递归扫描源目录并生成 .sdb 归档",
            "restore：校验归档并恢复目录、文件和元数据",
            "list：查看归档内容",
            "sdb_gui.exe：通过图形界面完成备份、还原和归档查看",
            "筛选：扩展名、名称、大小、修改时间",
            "扩展：RLE 压缩、密码加密、FNV-1a 校验",
        ],
    ),
    (
        "系统架构",
        [
            "命令行接口层：命令解析和参数校验",
            "图形界面层：页签、文件选择、状态提示",
            "业务逻辑层：扫描、筛选、备份、还原",
            "归档格式层：二进制读写、压缩、加密、校验",
            "文件系统适配层：Win32 API 遍历目录并处理元数据",
        ],
    ),
    (
        "归档格式",
        [
            "文件头：magic、版本、标记、创建时间、密码指纹、条目数量",
            "条目元数据：类型、相对路径、大小、修改时间、属性、校验和",
            "文件载荷：先压缩，再加密",
            "还原流程：解密、解压、校验、写入、恢复元数据",
        ],
    ),
    (
        "安全与健壮性",
        [
            "禁止将备份归档放在源目录内部",
            "还原时拒绝绝对路径、空路径、. 和 ..",
            "默认不覆盖已有文件，需要显式 --overwrite",
            "加密归档必须提供正确密码才能还原",
            "校验和不匹配时停止恢复",
        ],
    ),
    (
        "测试结果",
        [
            "mingw32-make 构建通过",
            "mingw32-make test 自动化测试通过",
            "覆盖备份、查看、还原、筛选、空目录、错误密码、GUI 构建产物",
            "当前测试输出：All tests passed.",
        ],
    ),
    (
        "项目总结",
        [
            "已完成 PDF 中基础要求：目录备份和数据还原",
            "已实现多个扩展要求：打包、压缩、加密、元数据、自定义备份、图形界面",
            "后续可扩展：增量备份、实时备份、任务进度条、网络备份",
        ],
    ),
]


NS = (
    'xmlns:a="http://schemas.openxmlformats.org/drawingml/2006/main" '
    'xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships" '
    'xmlns:p="http://schemas.openxmlformats.org/presentationml/2006/main"'
)


def x(text: str) -> str:
    return html.escape(text, quote=False)


def rels(items: list[tuple[str, str, str]]) -> str:
    body = "\n".join(
        f'<Relationship Id="{rid}" Type="{typ}" Target="{target}"/>'
        for rid, typ, target in items
    )
    return (
        '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n'
        '<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">\n'
        f"{body}\n"
        "</Relationships>"
    )


def sp_tree_prefix() -> str:
    return """
<p:nvGrpSpPr><p:cNvPr id="1" name=""/><p:cNvGrpSpPr/><p:nvPr/></p:nvGrpSpPr>
<p:grpSpPr><a:xfrm><a:off x="0" y="0"/><a:ext cx="0" cy="0"/><a:chOff x="0" y="0"/><a:chExt cx="0" cy="0"/></a:xfrm></p:grpSpPr>
"""


def text_shape(shape_id: int, name: str, left: int, top: int, width: int, height: int,
               text: str, size: int, bold: bool = False, color: str = "1F2937") -> str:
    bold_attr = ' b="1"' if bold else ""
    return f"""
<p:sp>
  <p:nvSpPr><p:cNvPr id="{shape_id}" name="{x(name)}"/><p:cNvSpPr/><p:nvPr/></p:nvSpPr>
  <p:spPr><a:xfrm><a:off x="{left}" y="{top}"/><a:ext cx="{width}" cy="{height}"/></a:xfrm><a:prstGeom prst="rect"><a:avLst/></a:prstGeom><a:noFill/></p:spPr>
  <p:txBody><a:bodyPr wrap="square"/><a:lstStyle/><a:p><a:r><a:rPr lang="zh-CN" sz="{size}"{bold_attr}><a:solidFill><a:srgbClr val="{color}"/></a:solidFill></a:rPr><a:t>{x(text)}</a:t></a:r><a:endParaRPr lang="zh-CN"/></a:p></p:txBody>
</p:sp>
"""


def body_shape(items: list[str]) -> str:
    paragraphs = []
    for item in items:
        paragraphs.append(
            '<a:p><a:pPr marL="342900" indent="-171450">'
            '<a:buChar char="•"/></a:pPr>'
            '<a:r><a:rPr lang="zh-CN" sz="2200">'
            '<a:solidFill><a:srgbClr val="374151"/></a:solidFill>'
            f"</a:rPr><a:t>{x(item)}</a:t></a:r>"
            '<a:endParaRPr lang="zh-CN"/></a:p>'
        )
    return f"""
<p:sp>
  <p:nvSpPr><p:cNvPr id="3" name="Body"/><p:cNvSpPr/><p:nvPr/></p:nvSpPr>
  <p:spPr><a:xfrm><a:off x="914400" y="1600200"/><a:ext cx="10363200" cy="4267200"/></a:xfrm><a:prstGeom prst="rect"><a:avLst/></a:prstGeom><a:noFill/></p:spPr>
  <p:txBody><a:bodyPr wrap="square"/><a:lstStyle/>{''.join(paragraphs)}</p:txBody>
</p:sp>
"""


def slide_xml(title: str, bullets: list[str]) -> str:
    return f"""<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<p:sld {NS}>
  <p:cSld>
    <p:bg><p:bgPr><a:solidFill><a:srgbClr val="F8FAFC"/></a:solidFill><a:effectLst/></p:bgPr></p:bg>
    <p:spTree>
      {sp_tree_prefix()}
      {text_shape(2, "Title", 685800, 548640, 10820400, 731520, title, 3600, True)}
      {body_shape(bullets)}
      {text_shape(4, "Footer", 914400, 6172200, 10363200, 365760, "Simple Data Backup", 1200, False, "64748B")}
    </p:spTree>
  </p:cSld>
  <p:clrMapOvr><a:masterClrMapping/></p:clrMapOvr>
</p:sld>"""


def content_types() -> str:
    slide_overrides = "\n".join(
        f'<Override PartName="/ppt/slides/slide{i}.xml" ContentType="application/vnd.openxmlformats-officedocument.presentationml.slide+xml"/>'
        for i in range(1, len(SLIDES) + 1)
    )
    return f"""<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">
  <Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>
  <Default Extension="xml" ContentType="application/xml"/>
  <Override PartName="/docProps/app.xml" ContentType="application/vnd.openxmlformats-officedocument.extended-properties+xml"/>
  <Override PartName="/docProps/core.xml" ContentType="application/vnd.openxmlformats-package.core-properties+xml"/>
  <Override PartName="/ppt/presentation.xml" ContentType="application/vnd.openxmlformats-officedocument.presentationml.presentation.main+xml"/>
  <Override PartName="/ppt/slideMasters/slideMaster1.xml" ContentType="application/vnd.openxmlformats-officedocument.presentationml.slideMaster+xml"/>
  <Override PartName="/ppt/slideLayouts/slideLayout1.xml" ContentType="application/vnd.openxmlformats-officedocument.presentationml.slideLayout+xml"/>
  <Override PartName="/ppt/theme/theme1.xml" ContentType="application/vnd.openxmlformats-officedocument.theme+xml"/>
  {slide_overrides}
</Types>"""


def presentation_xml() -> str:
    slide_ids = "\n".join(
        f'<p:sldId id="{255 + i}" r:id="rId{i + 1}"/>'
        for i in range(1, len(SLIDES) + 1)
    )
    return f"""<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<p:presentation {NS}>
  <p:sldMasterIdLst><p:sldMasterId id="2147483648" r:id="rId1"/></p:sldMasterIdLst>
  <p:sldIdLst>{slide_ids}</p:sldIdLst>
  <p:sldSz cx="12192000" cy="6858000" type="wide"/>
  <p:notesSz cx="6858000" cy="9144000"/>
</p:presentation>"""


def slide_master() -> str:
    return f"""<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<p:sldMaster {NS}>
  <p:cSld><p:spTree>{sp_tree_prefix()}</p:spTree></p:cSld>
  <p:clrMap bg1="lt1" tx1="dk1" bg2="lt2" tx2="dk2" accent1="accent1" accent2="accent2" accent3="accent3" accent4="accent4" accent5="accent5" accent6="accent6" hlink="hlink" folHlink="folHlink"/>
  <p:sldLayoutIdLst><p:sldLayoutId id="2147483649" r:id="rId1"/></p:sldLayoutIdLst>
  <p:txStyles><p:titleStyle/><p:bodyStyle/><p:otherStyle/></p:txStyles>
</p:sldMaster>"""


def slide_layout() -> str:
    return f"""<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<p:sldLayout {NS} type="blank" preserve="1">
  <p:cSld name="Blank"><p:spTree>{sp_tree_prefix()}</p:spTree></p:cSld>
  <p:clrMapOvr><a:masterClrMapping/></p:clrMapOvr>
</p:sldLayout>"""


def theme() -> str:
    return """<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<a:theme xmlns:a="http://schemas.openxmlformats.org/drawingml/2006/main" name="Simple Theme">
  <a:themeElements>
    <a:clrScheme name="Simple">
      <a:dk1><a:srgbClr val="111827"/></a:dk1><a:lt1><a:srgbClr val="FFFFFF"/></a:lt1>
      <a:dk2><a:srgbClr val="374151"/></a:dk2><a:lt2><a:srgbClr val="F8FAFC"/></a:lt2>
      <a:accent1><a:srgbClr val="2563EB"/></a:accent1><a:accent2><a:srgbClr val="059669"/></a:accent2>
      <a:accent3><a:srgbClr val="D97706"/></a:accent3><a:accent4><a:srgbClr val="DC2626"/></a:accent4>
      <a:accent5><a:srgbClr val="7C3AED"/></a:accent5><a:accent6><a:srgbClr val="0891B2"/></a:accent6>
      <a:hlink><a:srgbClr val="2563EB"/></a:hlink><a:folHlink><a:srgbClr val="7C3AED"/></a:folHlink>
    </a:clrScheme>
    <a:fontScheme name="Simple"><a:majorFont><a:latin typeface="Arial"/><a:ea typeface="Microsoft YaHei"/><a:cs typeface="Arial"/></a:majorFont><a:minorFont><a:latin typeface="Arial"/><a:ea typeface="Microsoft YaHei"/><a:cs typeface="Arial"/></a:minorFont></a:fontScheme>
    <a:fmtScheme name="Simple"><a:fillStyleLst><a:solidFill><a:schemeClr val="phClr"/></a:solidFill></a:fillStyleLst><a:lnStyleLst><a:ln w="9525"><a:solidFill><a:schemeClr val="phClr"/></a:solidFill></a:ln></a:lnStyleLst><a:effectStyleLst><a:effectStyle><a:effectLst/></a:effectStyle></a:effectStyleLst><a:bgFillStyleLst><a:solidFill><a:schemeClr val="phClr"/></a:solidFill></a:bgFillStyleLst></a:fmtScheme>
  </a:themeElements>
</a:theme>"""


def app_props() -> str:
    return f"""<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Properties xmlns="http://schemas.openxmlformats.org/officeDocument/2006/extended-properties" xmlns:vt="http://schemas.openxmlformats.org/officeDocument/2006/docPropsVTypes">
  <Application>Codex</Application><PresentationFormat>On-screen Show (16:9)</PresentationFormat><Slides>{len(SLIDES)}</Slides><Company></Company>
</Properties>"""


def core_props() -> str:
    return """<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<cp:coreProperties xmlns:cp="http://schemas.openxmlformats.org/package/2006/metadata/core-properties" xmlns:dc="http://purl.org/dc/elements/1.1/" xmlns:dcterms="http://purl.org/dc/terms/" xmlns:dcmitype="http://purl.org/dc/dcmitype/" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance">
  <dc:title>Simple Data Backup</dc:title><dc:creator>Codex</dc:creator><cp:lastModifiedBy>Codex</cp:lastModifiedBy><dcterms:created xsi:type="dcterms:W3CDTF">2026-07-06T00:00:00Z</dcterms:created><dcterms:modified xsi:type="dcterms:W3CDTF">2026-07-06T00:00:00Z</dcterms:modified>
</cp:coreProperties>"""


def main() -> None:
    OUT.parent.mkdir(parents=True, exist_ok=True)
    if OUT.exists():
        OUT.unlink()

    with zipfile.ZipFile(OUT, "w", compression=zipfile.ZIP_DEFLATED) as z:
        z.writestr("[Content_Types].xml", content_types())
        z.writestr(
            "_rels/.rels",
            rels(
                [
                    (
                        "rId1",
                        "http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument",
                        "ppt/presentation.xml",
                    ),
                    (
                        "rId2",
                        "http://schemas.openxmlformats.org/package/2006/relationships/metadata/core-properties",
                        "docProps/core.xml",
                    ),
                    (
                        "rId3",
                        "http://schemas.openxmlformats.org/officeDocument/2006/relationships/extended-properties",
                        "docProps/app.xml",
                    ),
                ]
            ),
        )
        z.writestr("docProps/app.xml", app_props())
        z.writestr("docProps/core.xml", core_props())
        z.writestr("ppt/presentation.xml", presentation_xml())

        presentation_rels = [
            (
                "rId1",
                "http://schemas.openxmlformats.org/officeDocument/2006/relationships/slideMaster",
                "slideMasters/slideMaster1.xml",
            )
        ]
        for i in range(1, len(SLIDES) + 1):
            presentation_rels.append(
                (
                    f"rId{i + 1}",
                    "http://schemas.openxmlformats.org/officeDocument/2006/relationships/slide",
                    f"slides/slide{i}.xml",
                )
            )
        z.writestr("ppt/_rels/presentation.xml.rels", rels(presentation_rels))
        z.writestr("ppt/slideMasters/slideMaster1.xml", slide_master())
        z.writestr(
            "ppt/slideMasters/_rels/slideMaster1.xml.rels",
            rels(
                [
                    (
                        "rId1",
                        "http://schemas.openxmlformats.org/officeDocument/2006/relationships/slideLayout",
                        "../slideLayouts/slideLayout1.xml",
                    ),
                    (
                        "rId2",
                        "http://schemas.openxmlformats.org/officeDocument/2006/relationships/theme",
                        "../theme/theme1.xml",
                    ),
                ]
            ),
        )
        z.writestr("ppt/slideLayouts/slideLayout1.xml", slide_layout())
        z.writestr(
            "ppt/slideLayouts/_rels/slideLayout1.xml.rels",
            rels(
                [
                    (
                        "rId1",
                        "http://schemas.openxmlformats.org/officeDocument/2006/relationships/slideMaster",
                        "../slideMasters/slideMaster1.xml",
                    )
                ]
            ),
        )
        z.writestr("ppt/theme/theme1.xml", theme())

        for i, (title, bullets) in enumerate(SLIDES, start=1):
            z.writestr(f"ppt/slides/slide{i}.xml", slide_xml(title, bullets))
            z.writestr(
                f"ppt/slides/_rels/slide{i}.xml.rels",
                rels(
                    [
                        (
                            "rId1",
                            "http://schemas.openxmlformats.org/officeDocument/2006/relationships/slideLayout",
                            "../slideLayouts/slideLayout1.xml",
                        )
                    ]
                ),
            )


if __name__ == "__main__":
    main()
