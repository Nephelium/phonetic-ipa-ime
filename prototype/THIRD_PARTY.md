# 来源、引用与许可

核对及改动日期：2026-10-23。代码作者与整理者：Nephelium。本清单补充水杉原有的 [THIRD_PARTY_NOTICES.txt](../THIRD_PARTY_NOTICES.txt)，不取代其中任何版权或许可证。

## 水杉输入法与依赖

| 对象 | 来源与版本 | 本项目的使用与许可 |
|---|---|---|
| 水杉 Windows | [metasequoiaime/msime-windows](https://github.com/metasequoiaime/msime-windows)，v0.11.1，`dd018f79858314ed542a47c528608752b13bd261` | 保留上游历史与版权，修改 TSF、Server、引擎适配和通用 UI，按 [GPLv3](../LICENSE) 提供对应源码。修改日期与路径见 [MODIFICATIONS.md](../MODIFICATIONS.md)。 |
| 输入引擎与 UI | 水杉仓内 `engine/`、`ui/` | 保留 [engine/UPSTREAM.md](../engine/UPSTREAM.md)、[engine/NOTICE.md](../engine/NOTICE.md) 及各目录许可证。 |
| 拼音与汉字候选词库 | [product-lock.json](../product-lock.json) 所指仓库、tag、源提交和每个文件 SHA-256 | 下载原始发行物并校验，不把用户学习数据库作为词库源。词库与辅助码声明沿用水杉原有清单。 |
| OpenCC、cpp-pinyin 等 | `.gitmodules` 记录地址，gitlink 固定提交 | OpenCC/cpp-pinyin 为 Apache-2.0。kenlm、utfcpp、miniaudio、fmt、spdlog、toml++、nlohmann/json、curl、zlib、SQLite、Boost 等的原文许可继续保留在上游文件与各目录，不统一改写为 GPL。 |
| 设置图片与皮肤 | 水杉原有资源，新加纸墨、青瓷、夜航三种纯色皮肤 | 水杉资源遵循原声明。新皮肤为本项目自绘布局和配色，无第三方角色图片。 |

## 本次新增的资源

所有收录文件的逐文件 SHA-256 见 [manifest.json](resources/manifest.json)。以下授权只针对本仓所列对象。

| 文件／对象 | 作者、来源与版本 | 修改及许可 |
|---|---|---|
| `ipa.cin` | Nephelium 自编国际音标码表，作者于 2026-10-23 确认 | 原始 UTF-8 文件不改写，按本仓 GPLv3 提供。502 条映射、91 个编码。它是键位方案，不作为语音学术语权威。 |
| `default.cin` | 由上述 CIN 经 `prepare_default_keymap.py` 生成 | 原 1 键输出移至反斜杠，补入 Shift+1–5 上标调值，保留原候选顺序；GPLv3。 |
| `beijing.tsv`、`beijing-toned.tsv` | Nephelium 的 PhoneticToolbox v3，M13 `frontend/src/modules/mandarin-ipa/ipa-data.json` 北京音列 | 提取字符、拼音、IPA，带调文件另保留原声调字段；仅本仓收录的整理快照按 GPLv3 提供，不据此改变整个 PhoneticToolbox 工程的许可。参考 Lee & Zee (2003)，书目与逐字映射核验分别说明如下。 |
| `word-readings.tsv` | [CC-CEDICT](https://www.mdbg.net/chinese/dictionary?page=cc-cedict)，MDBG 与贡献者；CEDICT 由 Paul Denisowski 于 1997 年创立 | 从 2026-10-21 导出快照提取简繁词形与带调拼音，保留歧义、去重，183,177 个词形／读音对；757 个非标准条目跳过。衍生文件继续采用 [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/)，原头部及修改说明在 `CC-CEDICT-README.txt`。 |
| `DoulosSIL-Regular.ttf` | SIL Global，Doulos SIL 7.000，[官方项目](https://software.sil.org/doulos/) | 原字体字节不变，主音标字体，私有加载。[OFL 1.1 全文与版权](resources/ipa/Doulos-OFL.txt)；Doulos、SIL 为保留字体名。 |
| `PTBIPAPlus-Regular.ttf` | SIL Global、Noto 作者、PhoneticToolbox 贡献者；PTB IPA Plus 1.000 | 复用 PhoneticToolbox 已更名的 Doulos SIL 7.000 派生字体，含补充组合与 Noto Sans Math 括号字形。本输入法没有再改字体字节，只用于缺字回退。[派生字体 OFL](resources/ipa/OFL-PTBIPAPlus.txt)、[Noto 原文](resources/ipa/OFL-Noto.txt)。 |
| `plus-catalog.json`、`plus-sources.json` | PhoneticToolbox v3 M17 国际音标表 Plus，625 个条目，IPA 351 / extIPA 209 / VoQS 65 | 复用 Unicode 输入序列、短名称、分类及布局数据，在原生控件中重新实现交互。未复制原论文、扫描图或录音。不同来源部分按下表保留各自许可；这些数据不因与程序一起分发而统一改为 GPL。 |

原始 M13 数据 SHA-256：`0213de35105d0251a4776f60461fee73b52a415620b711be1a69c1ac9343773f`。原 CIN SHA-256：`291d5ab9de8f64d9d51c7c6d0d479493dcc253ac1541be75ff2d510995a7d983`。CC-CEDICT 源 gzip SHA-256：`f50281886b7cedc9be90aa923886710711c2a67853e350b848733f8a2f75db47`。原目录数据与字体版本分别固定，完整记录在 [provenance.json](resources/provenance.json)。

## 图表及方法引用

| 部分 | 归属、引用与改写说明 |
|---|---|
| IPA | International Phonetic Association，2026 重印、内容修订至 2015/2005，[官方图表](https://www.internationalphoneticassociation.org/content/ipa-chart)。本项目交互图表衍生部分保持 [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/)，原 PDF 未随包。 |
| extIPA | International Clinical Phonetics and Linguistics Association，2025 图表，[ICPLA](https://www.icpla.org.uk/resources)。来源页列 [CC BY-SA 3.0](https://creativecommons.org/licenses/by-sa/3.0/) 并要求原图复制不作变更。此处为符号码位、分类及自有短解释的交互重排，未修改或分发原图。保留该来源声明，不将原图复制条件省略。 |
| VoQS 原体系 | Ball、Esling、Dickson，2016 修订表，论文在线 2017、卷期 2018。原论文与图版版权独立保留，本仓不打包这些文件。 |
| VoQS 中文名 | 王天恒译，*VoQS：音质符号（2016 中英双语版）*，2020-08-30，[Zenodo DOI](https://doi.org/10.5281/zenodo.10206204)，[CC BY 4.0](https://creativecommons.org/licenses/by/4.0/)。保留译者和原表作者，以现代 Unicode 和可点击布局呈现，不把译名许可扩大为原论文/图像许可。 |
| 北京音转写 | LEE W S, ZEE E. Standard Chinese (Beijing)[J]. Journal of the International Phonetic Association, 2003, 33(1): 109–112. [DOI](https://doi.org/10.1017/S0025100303001208)。历史列名对应此参考，原表逐字值由项目作者整理；书目核实不表示每个汉字都在论文中列出或已逐项复核。 |

中文术语、extIPA 历史版本、VoQS 发声体系及编码提案的完整书目继续保存在 [plus-sources.json](resources/ipa/plus-sources.json)，各条目标识与目录数据保持一致。补充参考如下。

- **extipa-lv-jiang-2013**：吕佳, 江荻. 国际音标扩展表的分类、命名与功能[J]. 听力学及言语疾病杂志, 2013, 21(6): 665-668. [来源](https://jasptl.haoyicn.cn/summary/19224?eng=0&type=history)。原论文版权保留，PDF和图表不随包分发。
- **ipa-chart-2026**：INTERNATIONAL PHONETIC ASSOCIATION. The International Phonetic Alphabet[EB/OL]. (2026)[2026-10-11]. https://www.internationalphoneticassociation.org/IPAcharts/common_files/pdfs/pdfs_IPA_charts_E/IPA_Kiel.pdf. [来源](https://www.internationalphoneticassociation.org/content/ipa-chart)。CC BY-SA 4.0。
- **extipa-chart-2025**：INTERNATIONAL CLINICAL PHONETICS AND LINGUISTICS ASSOCIATION. ExtIPA symbols for disordered speech[EB/OL]. (2025)[2026-10-11]. https://www.icpla.org.uk/resources. [来源](https://www.icpla.org.uk/resources)。CC BY-SA 3.0；原图不随包分发。
- **voqs-ball-2018**：BALL M J, ESLING J H, DICKSON B C. Revisions to the VoQS system for the transcription of voice quality[J]. Journal of the International Phonetic Association, 2018, 48(2): 165-171. https://doi.org/10.1017/S0025100317000159. [来源](https://doi.org/10.1017/S0025100317000159)。论文及图版版权保留，原 PDF 不随包分发。
- **voqs-zhihu-203037479**：王天恒, 译. VoQS: 音质符号 (2016中英双语版)[EB/OL]. (2020-08-30)[2026-10-11]. https://doi.org/10.5281/zenodo.10206204. [来源](https://doi.org/10.5281/zenodo.10206204)。CC BY 4.0（Zenodo 对应译表记录）；保留原表作者及改写说明，原图不随包分发。
- **unicode-cartouche-2024**：MILLER K, BALL M. Unicode request for ExtIPA cartouche: L2/24-182R[R/OL]. (2024-09-04)[2026-10-11]. https://www.unicode.org/L2/L2024/24182r-cartouche.pdf. [来源](https://www.unicode.org/L2/L2024/24182r-cartouche.pdf)。仅引用编码资料。
- **ipa-chart-zh-2007**：PHONETIC ASSOCIATION OF CHINA. The chart of the International Phonetic Alphabet in Chinese (2007)[J]. Journal of the International Phonetic Association, 2011, 41(2): 245. https://doi.org/10.1017/S0025100311000156. [来源](https://doi.org/10.1017/S0025100311000156)。中文名称据中文版图表核对；原图与文章不随包分发。
- **ipa-handbook-jiang-2008**：国际语音学会. 国际语音学会手册：国际音标使用指南[M]. 江荻, 译. 上海: 上海教育出版社, 2008. [来源](https://ci.nii.ac.jp/ncid/BB01991047)。书籍版权保留；只使用自主短解释、书目信息和页码，原书与扫描图不随包。
- **voice-quality-esling-2019**：ESLING J H, MOISIK S R, BENNER A, et al. Voice quality: The laryngeal articulator model[M]. Cambridge: Cambridge University Press, 2019. https://doi.org/10.1017/9781108696555. [来源](https://doi.org/10.1017/9781108696555)。书籍版权保留；自主概述并列准确章节页码，原PDF及图像不随包。
- **extipa-ball-2018**：BALL M J, HOWARD S J, MILLER K. Revisions to the extIPA chart[J]. Journal of the International Phonetic Association, 2018, 48(2): 155-164. https://doi.org/10.1017/S0025100317000147. [来源](https://doi.org/10.1017/S0025100317000147)。原论文与图表不随包，条目含义自主概述。
- **extipa-ball-2024**：BALL M J. Changes to certain extIPA diacritics[J]. Clinical Linguistics & Phonetics, 2024, 38(7): 692-695. https://doi.org/10.1080/02699206.2024.2365205. [来源](https://doi.org/10.1080/02699206.2024.2365205)。原文CC BY-NC-ND 4.0；仅引用与自主解释，未复制或修改原图。
- **extipa-ball-2025-reply**：BALL M J. Reply to letter concerning the revisions to some diacritics of the extensions to the IPA (extIPA), Ball (2024)[J]. Clinical Linguistics & Phonetics, 2025, 39(9): 911-912. https://doi.org/10.1080/02699206.2025.2489577. [来源](https://doi.org/10.1080/02699206.2025.2489577)。原文版权与许可保留；仅书目和版本差异说明，原PDF不随包。

## 分发范围

本仓公开本次衍生实现、作者提供的码表与限定数据快照、可再分发的字体及来源声明。字体 OFL、CC 数据许可和程序 GPL 分别保留。系统字体不随包，用户输入、活动码表、个人配置、学习词库、工具链缓存与本机路径不进入提交。未发布安装包或承诺真实宿主验收。
