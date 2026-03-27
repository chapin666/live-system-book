#!/bin/bash
# 构建 live-system-book 电子书

set -e

BOOK_DIR="/root/.openclaw/workspace/live-system-book"
SRC_DIR="$BOOK_DIR/src"
OUTPUT_DIR="$BOOK_DIR/ebook"

echo "📚 开始构建电子书..."
mkdir -p "$OUTPUT_DIR"

# 按顺序合并所有章节
cat > "$OUTPUT_DIR/live-system-book.md" << 'EOF'
---
title: "音视频开发实战：从播放器到连麦"
author: "live-system-book"
date: "2026"
lang: zh-CN
---

EOF

# 定义章节顺序
chapters=(
    "README.md"
    "chapter-01.md"
    "chapter-02.md"
    "chapter-03.md"
    "project-01.md"
    "chapter-04.md"
    "chapter-05.md"
    "chapter-06.md"
    "project-02.md"
    "chapter-07.md"
    "chapter-08.md"
    "project-03.md"
    "chapter-09.md"
    "chapter-10.md"
    "chapter-11.md"
    "chapter-12.md"
    "chapter-13.md"
    "chapter-14.md"
    "project-04.md"
    "chapter-15.md"
    "chapter-16.md"
    "project-05.md"
    "chapter-17.md"
    "chapter-18.md"
    "chapter-19.md"
    "project-06.md"
    "chapter-20.md"
    "chapter-21.md"
    "project-07.md"
    "chapter-22.md"
    "chapter-23.md"
    "project-08.md"
    "chapter-24.md"
    "chapter-25.md"
    "project-09.md"
    "chapter-26.md"
    "project-10.md"
    "chapter-27.md"
    "chapter-28.md"
    "chapter-29.md"
    "chapter-30.md"
    "chapter-31.md"
    "project-11.md"
    "GLOSSARY.md"
    "appendix-a-cpp11-thread.md"
)

# 合并所有章节
echo "📝 合并章节内容..."
for chapter in "${chapters[@]}"; do
    if [ -f "$SRC_DIR/$chapter" ]; then
        echo "" >> "$OUTPUT_DIR/live-system-book.md"
        echo "<!-- $chapter -->" >> "$OUTPUT_DIR/live-system-book.md"
        echo "" >> "$OUTPUT_DIR/live-system-book.md"
        cat "$SRC_DIR/$chapter" >> "$OUTPUT_DIR/live-system-book.md"
        echo "" >> "$OUTPUT_DIR/live-system-book.md"
        echo -e "\\n\\n---\\n\\n" >> "$OUTPUT_DIR/live-system-book.md"
    fi
done

echo "✅ 合并完成: $OUTPUT_DIR/live-system-book.md"

# 生成 EPUB
echo "📖 生成 EPUB 格式..."
pandoc "$OUTPUT_DIR/live-system-book.md" \
    -o "$OUTPUT_DIR/live-system-book.epub" \
    --metadata title="音视频开发实战：从播放器到连麦" \
    --metadata author="live-system-book" \
    --metadata lang="zh-CN" \
    --toc \
    --toc-depth=3 \
    --epub-chapter-level=2 \
    -f markdown \
    --highlight-style=tango \
    2>/dev/null || echo "⚠️ EPUB 生成遇到问题"

# 生成 HTML（单文件）
echo "🌐 生成 HTML 格式..."
pandoc "$OUTPUT_DIR/live-system-book.md" \
    -o "$OUTPUT_DIR/live-system-book.html" \
    --metadata title="音视频开发实战：从播放器到连麦" \
    --standalone \
    --toc \
    --toc-depth=3 \
    --css=https://cdn.jsdelivr.net/npm/github-markdown-css@5/github-markdown.min.css \
    -f markdown \
    --highlight-style=tango \
    2>/dev/null || echo "⚠️ HTML 生成遇到问题"

# 生成 PDF（如果系统有 TeX 环境）
if command -v xelatex &> /dev/null; then
    echo "📄 生成 PDF 格式..."
    pandoc "$OUTPUT_DIR/live-system-book.md" \
        -o "$OUTPUT_DIR/live-system-book.pdf" \
        --metadata title="音视频开发实战：从播放器到连麦" \
        --metadata author="live-system-book" \
        --metadata lang="zh-CN" \
        --toc \
        --toc-depth=2 \
        -V CJKmainfont="Noto Sans CJK SC" \
        -V geometry:margin=2.5cm \
        -V colorlinks=true \
        -f markdown \
        --pdf-engine=xelatex \
        2>/dev/null || echo "⚠️ PDF 生成遇到问题（可能需要安装 TeX 环境和中文字体）"
else
    echo "⏭️  跳过 PDF（未安装 xelatex）"
fi

echo ""
echo "✅ 电子书构建完成！"
echo "📂 输出目录: $OUTPUT_DIR"
echo ""
ls -lh "$OUTPUT_DIR"
