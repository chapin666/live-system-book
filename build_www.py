#!/usr/bin/env python3
"""构建 live-system-book 静态网站"""

import os
import re
import subprocess
import re
import shutil

# 获取脚本所在目录
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
BOOK_DIR = SCRIPT_DIR

# 源文件位置（优先 src/，其次原始位置）
SRC_DIR = f"{BOOK_DIR}/src"
WWW_DIR = f"{BOOK_DIR}/ebook/www"

# (源路径, 输出HTML名, 标题, Part分组)
CHAPTERS = [
    ("README.md", "README.html", "关于本书", None),
    ("chapter-01/README.md", "chapter-01.html", "Ch1: 视频基础", "Part 1: 播放器基础"),
    ("chapter-02/README.md", "chapter-02.html", "Ch2: 第一个播放器", None),
    ("chapter-03/README.md", "chapter-03.html", "Ch3: Pipeline工程化", None),
    ("project-01/README.md", "project-01.html", "P1: 完整本地播放器", None),
    ("chapter-04/README.md", "chapter-04.html", "Ch4: 为什么卡顿？", None),
    ("chapter-05/README.md", "chapter-05.html", "Ch5: C++11多线程", None),
    ("chapter-06/README.md", "chapter-06.html", "Ch6: 异步播放器", None),
    ("project-02/README.md", "project-02.html", "P2: 网络点播播放器", None),
    ("chapter-07/README.md", "chapter-07.html", "Ch7: 网络基础", None),
    ("chapter-08/README.md", "chapter-08.html", "Ch8: 直播vs点播", None),
    ("project-03/README.md", "project-03.html", "P3: 直播观众端", None),
    ("chapter-09/README.md", "chapter-09.html", "Ch9: 硬件解码", None),
    ("chapter-10/README.md", "chapter-10.html", "Ch10: 音视频采集", None),
    ("chapter-11/README.md", "chapter-11.html", "Ch11: 音频3A处理", None),
    ("chapter-12/README.md", "chapter-12.html", "Ch12: 编码与推流", None),
    ("chapter-13/README.md", "chapter-13.html", "Ch13: 视频编码进阶", "Part 2: 主播端进阶"),
    ("chapter-14/README.md", "chapter-14.html", "Ch14: 高级采集", None),
    ("project-04/README.md", "project-04.html", "P4: 采集与预览", None),
    ("chapter-15/README.md", "chapter-15.html", "Ch15: 音频编码", None),
    ("chapter-16/README.md", "chapter-16.html", "Ch16: 美颜与滤镜", None),
    ("project-05/README.md", "project-05.html", "P5: 完整主播端", None),
    ("chapter-17/README.md", "chapter-17.html", "Ch17: 网络编程基础", "Part 3: 实时连麦"),
    ("chapter-18/README.md", "chapter-18.html", "Ch18: UDP与实时传输", None),
    ("chapter-19/README.md", "chapter-19.html", "Ch19: NAT穿透与P2P", None),
    ("project-06/README.md", "project-06.html", "P6: P2P通话工具", None),
    ("chapter-20/README.md", "chapter-20.html", "Ch20: WebRTC标准", None),
    ("chapter-21/README.md", "chapter-21.html", "Ch21: WebRTC Native", None),
    ("project-07/README.md", "project-07.html", "P7: WebRTC连麦", None),
    ("chapter-22/README.md", "chapter-22.html", "Ch22: SFU转发", None),
    ("chapter-23/README.md", "chapter-23.html", "Ch23: 多人房间", None),
    ("project-08/README.md", "project-08.html", "P8: 多人会议", None),
    ("chapter-24/README.md", "chapter-24.html", "Ch24: MCU混音混画", "Part 4: 服务端架构"),
    ("chapter-25/README.md", "chapter-25.html", "Ch25: 录制与回放", None),
    ("project-09/README.md", "project-09.html", "P9: 录制回放系统", None),
    ("chapter-26/README.md", "chapter-26.html", "Ch26: 最新协议探索", None),
    ("project-10/README.md", "project-10.html", "P10: 完整服务端", None),
    ("chapter-27/README.md", "chapter-27.html", "Ch27: 质量监控", "Part 5: 生产部署"),
    ("chapter-28/README.md", "chapter-28.html", "Ch28: 安全防护", None),
    ("chapter-29/README.md", "chapter-29.html", "Ch29: 性能调优", None),
    ("chapter-30/README.md", "chapter-30.html", "Ch30: Docker容器化", None),
    ("chapter-31/README.md", "chapter-31.html", "Ch31: K8s部署", None),
    ("project-11/README.md", "project-11.html", "P11: 生产级部署", None),
    ("appendix-a-cpp11-thread.md", "appendix-a-cpp11-thread.html", "附录A: C++11线程", "附录"),
    ("GLOSSARY.md", "GLOSSARY.html", "词汇表", None),
]

def find_source_file(path):
    """查找源文件，优先 src/ 目录"""
    # 获取基础文件名（如 chapter-09.md 或 README.md）
    basename = os.path.basename(path)
    
    # 对于 chapter-XX/README.md 或 project-XX/README.md，映射为 src/chapter-XX.md
    if '/' in path:
        # path 格式: chapter-09/README.md -> 映射为 chapter-09.md
        dir_name = path.split('/')[0]  # chapter-09
        src_path = f"{SRC_DIR}/{dir_name}.md"
        if os.path.exists(src_path):
            return src_path
    
    # 对于其他文件，直接映射到 src/ 目录
    src_path = f"{SRC_DIR}/{basename}"
    if os.path.exists(src_path):
        return src_path
    
    # 尝试原始位置
    orig_path = f"{BOOK_DIR}/{path}"
    if os.path.exists(orig_path):
        return orig_path
    
    return None

def generate_nav(current_html=None):
    """生成导航侧边栏"""
    nav = ['<h1><a href="index.html">📺 音视频开发实战</a></h1>', '<ul>']
    
    current_part = None
    for path, html_name, title, part in CHAPTERS:
        if part and part != current_part:
            nav.append(f'<li class="part">{part}</li>')
            current_part = part
        
        active = ' class="active"' if html_name == current_html else ''
        nav.append(f'<li><a href="{html_name}"{active}>{title}</a></li>')
    
    nav.append('</ul>')
    return '\n'.join(nav)

def convert_file(src_file, html_name, chapter_dir=None):
    """转换单个 Markdown 文件为 HTML"""
    output = f"{WWW_DIR}/{html_name}"
    
    # 使用 pandoc 转换
    result = subprocess.run([
        'pandoc', src_file, '-o', output,
        '--standalone',
        '--css=style.css',
        '--highlight-style=tango',
        '-f', 'markdown',
        '--metadata', 'title=音视频开发实战'
    ], capture_output=True, text=True)
    
    if result.returncode != 0:
        print(f"  ⚠️  {html_name}: {result.stderr[:100]}")
        return False
    
    # 读取生成的 HTML
    with open(output, 'r', encoding='utf-8') as f:
        content = f.read()
    
    # 修复图片路径：将 ./diagrams/ 替换为章节目录的相对路径
    if chapter_dir:
        # 从 src/chapter-XX.md 或 chapter-XX/README.md 提取章节名
        chapter_name = chapter_dir  # chapter-01, project-01 等
        content = re.sub(r'src="\.\/diagrams\/', f'src="{chapter_name}/diagrams/', content)
        content = re.sub(r'src="diagrams\/', f'src="{chapter_name}/diagrams/', content)
    
    # 生成导航
    nav = generate_nav(html_name)
    
    # 注入 Mermaid.js 支持（使用兼容方式加载）
    mermaid_script = '''<script src="https://cdn.jsdelivr.net/npm/mermaid@10/dist/mermaid.min.js"></script>
<script>
  document.addEventListener('DOMContentLoaded', function() {
    // 将 pre.mermaid 转换为 div.mermaid（解决 pandoc 转义问题）
    document.querySelectorAll('pre.mermaid').forEach(function(pre) {
      var div = document.createElement('div');
      div.className = 'mermaid';
      div.textContent = pre.textContent;
      pre.parentNode.replaceChild(div, pre);
    });
    mermaid.initialize({ 
      startOnLoad: true, 
      theme: 'default',
      securityLevel: 'loose',
      flowchart: { useMaxWidth: true, htmlLabels: true }
    });
  });
</script>
'''
    content = content.replace('</body>', f'{mermaid_script}</body>')
    
    # 修复 mermaid 代码块：pandoc 会包装成 <pre class="mermaid"><code>...，需要改为纯 <pre class="mermaid">...
    # 将 <pre class="mermaid"><code>...</code></pre> 改为 <pre class="mermaid">...</pre>
    content = re.sub(r'<pre class="mermaid"><code>(.*?)</code></pre>', r'<pre class="mermaid">\1</pre>', content, flags=re.DOTALL)
    
    # 注入侧边栏
    content = content.replace('<body>', f'<body>\n<div class="sidebar">\n{nav}\n</div>\n<div class="main">')
    content = content.replace('</body>', '</div>\n</body>')
    
    # 写入
    with open(output, 'w', encoding='utf-8') as f:
        f.write(content)
    
    print(f"  ✅ {html_name}")
    return True

def create_css():
    """创建 CSS 样式文件"""
    css_content = '''
:root { --primary: #2563eb; --bg: #fff; --text: #1f2937; --sidebar-bg: #f8fafc; --border: #e5e7eb; --code-bg: #f3f4f6; }
* { box-sizing: border-box; }
body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "Noto Sans SC", sans-serif; line-height: 1.8; color: var(--text); margin: 0; display: flex; }
.sidebar { width: 280px; background: var(--sidebar-bg); border-right: 1px solid var(--border); position: fixed; height: 100vh; overflow-y: auto; padding: 20px; }
.sidebar h1 { font-size: 18px; margin: 0 0 15px 0; padding-bottom: 10px; border-bottom: 2px solid var(--primary); }
.sidebar h1 a { color: var(--primary); text-decoration: none; }
.sidebar ul { list-style: none; padding: 0; margin: 0; }
.sidebar li { margin: 3px 0; }
.sidebar a { color: var(--text); text-decoration: none; display: block; padding: 5px 10px; border-radius: 4px; font-size: 13px; }
.sidebar a:hover, .sidebar a.active { background: var(--primary); color: white; }
.sidebar .part { font-weight: 600; margin-top: 12px; padding: 6px 10px; background: #e2e8f0; border-radius: 4px; font-size: 13px; }
.main { margin-left: 280px; flex: 1; max-width: 900px; padding: 30px 50px; }
h1 { font-size: 28px; border-bottom: 2px solid var(--border); padding-bottom: 10px; }
h2 { font-size: 22px; margin-top: 35px; border-bottom: 1px solid #eee; padding-bottom: 8px; }
h3 { font-size: 18px; margin-top: 25px; }
pre { background: var(--code-bg); padding: 16px; border-radius: 8px; overflow-x: auto; }
code { font-family: "SF Mono", Monaco, Consolas, monospace; font-size: 14px; }
pre code { background: none; padding: 0; }
p code, li code { background: var(--code-bg); padding: 2px 6px; border-radius: 4px; }

/* Mermaid 图表样式 */
.mermaid { background: #fff !important; border: 1px solid var(--border); margin: 20px 0; }
.mermaid svg { max-width: 100%; height: auto; display: block; margin: 0 auto; }

/* 隐藏 Mermaid 渲染前的代码 */
.mermaid:not([data-processed="true"]) { color: transparent; }

table { width: 100%; border-collapse: collapse; margin: 20px 0; }
th, td { border: 1px solid var(--border); padding: 10px 14px; text-align: left; }
th { background: var(--sidebar-bg); font-weight: 600; }
tr:nth-child(even) { background: #fafafa; }
img { max-width: 100%; height: auto; display: block; }
figure { margin: 20px 0; text-align: center; }
figcaption { margin-top: 10px; font-size: 14px; color: #666; }
blockquote { border-left: 4px solid var(--primary); margin: 20px 0; padding: 10px 20px; background: var(--sidebar-bg); }
@media print { .sidebar { display: none; } .main { margin-left: 0; max-width: none; } }
@media (max-width: 900px) { .sidebar { width: 220px; } .main { margin-left: 220px; padding: 20px; } }
'''
    with open(f"{WWW_DIR}/style.css", 'w') as f:
        f.write(css_content)
    print("  ✅ style.css")

def main():
    print("🌐 构建静态网站...")
    
    # 清理并创建目录
    if os.path.exists(WWW_DIR):
        shutil.rmtree(WWW_DIR)
    os.makedirs(WWW_DIR, exist_ok=True)
    
    # 复制所有 diagrams 目录到输出目录
    print("  📁 复制图片资源...")
    for item in os.listdir(BOOK_DIR):
        if item.startswith('chapter-') or item.startswith('project-'):
            diagrams_src = f"{BOOK_DIR}/{item}/diagrams"
            if os.path.exists(diagrams_src):
                diagrams_dst = f"{WWW_DIR}/{item}/diagrams"
                shutil.copytree(diagrams_src, diagrams_dst)
                print(f"    ✅ {item}/diagrams")
    
    # 创建 CSS
    create_css()
    
    # 转换所有章节
    for path, html_name, title, part in CHAPTERS:
        src = find_source_file(path)
        if src:
            # 提取章节目录名（如 chapter-01 或 project-01）
            chapter_dir = None
            if '/' in path:
                chapter_dir = path.split('/')[0]  # chapter-01/README.md -> chapter-01
            elif path.startswith('chapter-') or path.startswith('project-'):
                chapter_dir = path.replace('.md', '')  # chapter-01.md -> chapter-01
            convert_file(src, html_name, chapter_dir)
        else:
            print(f"  ⚠️  跳过: {path}")
    
    # 创建 index.html (从 README.html 复制)
    readme_html = f"{WWW_DIR}/README.html"
    index_html = f"{WWW_DIR}/index.html"
    if os.path.exists(readme_html):
        shutil.copy(readme_html, index_html)
        print(f"  ✅ index.html")
    
    print(f"\n✅ 静态网站构建完成！")
    print(f"📂 位置: {WWW_DIR}")

if __name__ == '__main__':
    main()
