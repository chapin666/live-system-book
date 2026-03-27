#!/bin/bash
# 构建静态网站版本

set -e

BOOK_DIR="/root/.openclaw/workspace/live-system-book"
SRC_DIR="$BOOK_DIR/src"
WWW_DIR="$BOOK_DIR/ebook/www"

echo "🌐 构建静态网站..."
mkdir -p "$WWW_DIR"

# 创建样式文件
cat > "$WWW_DIR/style.css" << 'EOF'
:root {
    --primary: #2563eb;
    --bg: #ffffff;
    --text: #1f2937;
    --sidebar-bg: #f8fafc;
    --border: #e5e7eb;
    --code-bg: #f3f4f6;
}

* { box-sizing: border-box; }

body {
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "Noto Sans SC", "PingFang SC", "Microsoft YaHei", sans-serif;
    line-height: 1.8;
    color: var(--text);
    margin: 0;
    display: flex;
    min-height: 100vh;
}

.sidebar {
    width: 280px;
    background: var(--sidebar-bg);
    border-right: 1px solid var(--border);
    position: fixed;
    height: 100vh;
    overflow-y: auto;
    padding: 20px;
}

.sidebar h1 {
    font-size: 18px;
    margin: 0 0 20px 0;
    padding-bottom: 15px;
    border-bottom: 2px solid var(--primary);
}

.sidebar h1 a {
    color: var(--primary);
    text-decoration: none;
}

.sidebar ul {
    list-style: none;
    padding: 0;
    margin: 0;
}

.sidebar li {
    margin: 4px 0;
}

.sidebar a {
    color: var(--text);
    text-decoration: none;
    display: block;
    padding: 6px 10px;
    border-radius: 4px;
    font-size: 14px;
}

.sidebar a:hover, .sidebar a.active {
    background: var(--primary);
    color: white;
}

.sidebar .part {
    font-weight: 600;
    margin-top: 15px;
    padding: 8px 10px;
    background: #e2e8f0;
    border-radius: 4px;
}

.main {
    margin-left: 280px;
    flex: 1;
    max-width: 900px;
    padding: 40px 60px;
}

h1 { font-size: 32px; border-bottom: 2px solid var(--border); padding-bottom: 10px; }
h2 { font-size: 24px; margin-top: 40px; border-bottom: 1px solid #eee; padding-bottom: 8px; }
h3 { font-size: 20px; margin-top: 30px; }

pre {
    background: var(--code-bg);
    padding: 16px;
    border-radius: 8px;
    overflow-x: auto;
}

code {
    font-family: "SF Mono", Monaco, Consolas, monospace;
    font-size: 14px;
}

pre code { background: none; padding: 0; }

p code, li code {
    background: var(--code-bg);
    padding: 2px 6px;
    border-radius: 4px;
}

table {
    width: 100%;
    border-collapse: collapse;
    margin: 20px 0;
}

th, td {
    border: 1px solid var(--border);
    padding: 10px 14px;
    text-align: left;
}

th { background: var(--sidebar-bg); font-weight: 600; }

tr:nth-child(even) { background: #fafafa; }

img { max-width: 100%; height: auto; }

blockquote {
    border-left: 4px solid var(--primary);
    margin: 20px 0;
    padding: 10px 20px;
    background: var(--sidebar-bg);
}

@media print {
    .sidebar { display: none; }
    .main { margin-left: 0; max-width: none; }
}

@media (max-width: 900px) {
    .sidebar { width: 220px; }
    .main { margin-left: 220px; padding: 20px; }
}
EOF

# 生成导航 HTML
NAV_HTML="$WWW_DIR/nav.html"
cat > "$NAV_HTML" <> EOF
<h1><a href="index.html">📺 音视频开发实战</a></h1>
<ul>
EOF

# 添加章节链接
add_link() {
    local file="$1"
    local title="$2"
    local html="${file%.md}.html"
    echo "    <li><a href=\"$html\" id=\"nav-$html\">$title</a></li>" >> "$NAV_HTML"
}

echo "    <li class='part'>介绍</li>" >> "$NAV_HTML"
add_link "README.md" "关于本书"

echo "    <li class='part'>Part 1: 播放器基础</li>" >> "$NAV_HTML"
add_link "chapter-01.md" "Ch1: 视频基础"
add_link "chapter-02.md" "Ch2: 第一个播放器"
add_link "chapter-03.md" "Ch3: Pipeline工程化"
add_link "project-01.md" "P1: 完整本地播放器"
add_link "chapter-04.md" "Ch4: 为什么卡顿？"
add_link "chapter-05.md" "Ch5: C++11多线程"
add_link "chapter-06.md" "Ch6: 异步播放器"
add_link "project-02.md" "P2: 网络点播播放器"
add_link "chapter-07.md" "Ch7: 网络基础"
add_link "chapter-08.md" "Ch8: 直播vs点播"
add_link "project-03.md" "P3: 直播观众端"
add_link "chapter-09.md" "Ch9: 硬件解码"
add_link "chapter-10.md" "Ch10: 音视频采集"
add_link "chapter-11.md" "Ch11: 音频3A处理"
add_link "chapter-12.md" "Ch12: 编码与推流"

echo "    <li class='part'>Part 2: 主播端进阶</li>" >> "$NAV_HTML"
add_link "chapter-13.md" "Ch13: 视频编码进阶"
add_link "chapter-14.md" "Ch14: 高级采集"
add_link "project-04.md" "P4: 采集与预览"
add_link "chapter-15.md" "Ch15: 音频编码"
add_link "chapter-16.md" "Ch16: 美颜与滤镜"
add_link "project-05.md" "P5: 完整主播端"

echo "    <li class='part'>Part 3: 实时连麦</li>" >> "$NAV_HTML"
add_link "chapter-17.md" "Ch17: 网络编程基础"
add_link "chapter-18.md" "Ch18: UDP与实时传输"
add_link "chapter-19.md" "Ch19: NAT穿透与P2P"
add_link "project-06.md" "P6: P2P通话工具"
add_link "chapter-20.md" "Ch20: WebRTC标准"
add_link "chapter-21.md" "Ch21: WebRTC Native"
add_link "project-07.md" "P7: WebRTC连麦"
add_link "chapter-22.md" "Ch22: SFU转发"
add_link "chapter-23.md" "Ch23: 多人房间"
add_link "project-08.md" "P8: 多人会议"

echo "    <li class='part'>Part 4: 服务端架构</li>" >> "$NAV_HTML"
add_link "chapter-24.md" "Ch24: MCU混音混画"
add_link "chapter-25.md" "Ch25: 录制与回放"
add_link "project-09.md" "P9: 录制回放系统"
add_link "chapter-26.md" "Ch26: 最新协议探索"
add_link "project-10.md" "P10: 完整服务端"

echo "    <li class='part'>Part 5: 生产部署</li>" >> "$NAV_HTML"
add_link "chapter-27.md" "Ch27: 质量监控"
add_link "chapter-28.md" "Ch28: 安全防护"
add_link "chapter-29.md" "Ch29: 性能调优"
add_link "chapter-30.md" "Ch30: Docker容器化"
add_link "chapter-31.md" "Ch31: K8s部署"
add_link "project-11.md" "P11: 生产级部署"

echo "    <li class='part'>附录</li>" >> "$NAV_HTML"
add_link "appendix-a-cpp11-thread.md" "附录A: C++11线程"
add_link "GLOSSARY.md" "词汇表"

echo "</ul>" >> "$NAV_HTML"

# 转换每个章节为 HTML
convert_file() {
    local src="$1"
    local name=$(basename "$src" .md)
    local html="$WWW_DIR/${name}.html"
    
    echo "  📄 $name.html"
    
    pandoc "$src" -o "$html" \
        --standalone \
        --template=/dev/null \
        --css=style.css \
        --highlight-style=tango \
        -f markdown \
        --metadata title="音视频开发实战" 2>/dev/null
    
    # 注入侧边栏和样式
    awk -v nav="$NAV_HTML" '
        /<body>/ {
            print
            print "<div class=\"sidebar\">"
            while ((getline line < nav) > 0) print line
            close(nav)
            print "</div>"
            print "<div class=\"main\">"
            next
        }
        /<\/body>/ {
            print "</div>"
            print
            next
        }
        { print }
    ' "$html" > "${html}.tmp" && mv "${html}.tmp" "$html"
    
    # 高亮当前页面
    local current="${name}.html"
    sed -i "s|id=\"nav-$current\"|id=\"nav-$current\" class=\"active\"|" "$html"
}

# 转换所有文件
for file in "$SRC_DIR"/*.md; do
    convert_file "$file"
done

# 复制 index.html
ln -sf README.html "$WWW_DIR/index.html"

echo ""
echo "✅ 静态网站构建完成！"
echo "📂 位置: $WWW_DIR"
echo "🌐 用浏览器打开: file://$WWW_DIR/index.html"
