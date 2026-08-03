#!/usr/bin/env python3
"""Apply ROCgalgame's aspect and safe-text hooks to OnscripterYuri sources."""

from pathlib import Path
import re
import sys


def replace_once(text: str, pattern: str, replacement: str, label: str) -> str:
    updated, count = re.subn(pattern, replacement, text, count=1, flags=re.MULTILINE | re.DOTALL)
    if count != 1:
        raise SystemExit(f"[ons_patch] expected one {label} block, found {count}")
    return updated


def patch_cpp(source: str) -> str:
    source = source.replace(
        "roc_gkd_fullscreen = fullscreen_mode && std::getenv",
        "roc_gkd_fullscreen = fullscreen_mode && isnan(sharpness) && std::getenv",
        1,
    )

    source = replace_once(
        source,
        r"    const bool fill_height = fullscreen_mode && roc_aspect_mode == ROC_ASPECT_FILL_HEIGHT &&\n"
        r"                             !\(force_window_height && force_window_width\);\n"
        r"    bool shouldAlign = !fill_height && \(!stretch_mode \|\| !fullscreen_mode\) &&\n"
        r"                       \(!force_window_height \|\| !force_window_width\);\n\n"
        r"    if \(fill_height\) \{\n"
        r"        screen_device_height = height;\n"
        r"        screen_device_width = static_cast<int>\(ceil\(\n"
        r"            static_cast<double>\(screen_width\) \* height / std::max\(1, screen_height\)\)\);\n"
        r"    \} else if \(shouldAlign\) \{\n"
        r"        alignAspectRatio\(screen_width, screen_height, screen_device_width, screen_device_height\);\n"
        r"    \}",
        """    const bool crop_aspect = fullscreen_mode &&
                             !(force_window_height && force_window_width);
    const bool fill_height = crop_aspect &&
                             roc_aspect_mode == ROC_ASPECT_FILL_HEIGHT;
    const bool fill_width = crop_aspect &&
                            roc_aspect_mode == ROC_ASPECT_FILL_WIDTH;
    bool shouldAlign = !fill_height && !fill_width &&
                       (!stretch_mode || !fullscreen_mode) &&
                       (!force_window_height || !force_window_width);

    if (fill_height) {
        screen_device_height = height;
        screen_device_width = static_cast<int>(ceil(
            static_cast<double>(screen_width) * height / std::max(1, screen_height)));
    } else if (fill_width) {
        screen_device_width = width;
        screen_device_height = static_cast<int>(ceil(
            static_cast<double>(screen_height) * width / std::max(1, screen_width)));
    } else if (shouldAlign) {
        alignAspectRatio(screen_width, screen_height, screen_device_width, screen_device_height);
    }""",
        "render rectangle calculation",
    )

    source = replace_once(
        source,
        r"void ONScripter::captureRocSentenceLayout\(\)\n\{.*?\n\}\n\nvoid ONScripter::drawRocVirtualMouseCursor",
        """void ONScripter::captureRocSentenceLayout()
{
    roc_sentence_top_x_original = sentence_font.top_xy[0];
    roc_sentence_num_x_original = sentence_font.num_xy[0];
    roc_sentence_top_y_original = sentence_font.top_xy[1];
    roc_sentence_num_y_original = sentence_font.num_xy[1];
    applyRocSentenceSafeArea();
}

void ONScripter::applyRocSentenceSafeArea()
{
    sentence_font.top_xy[0] = roc_sentence_top_x_original;
    sentence_font.num_xy[0] = roc_sentence_num_x_original;
    sentence_font.top_xy[1] = roc_sentence_top_y_original;
    sentence_font.num_xy[1] = roc_sentence_num_y_original;
    const bool crop_aspect = fullscreen_mode &&
                             !(force_window_height && force_window_width);
    const bool crop_horizontal = crop_aspect &&
                                 roc_aspect_mode == ROC_ASPECT_FILL_HEIGHT;
    const bool crop_vertical = crop_aspect &&
                               roc_aspect_mode == ROC_ASPECT_FILL_WIDTH;
    if ((!crop_horizontal && !crop_vertical) ||
        sentence_font.getTateyokoMode() != FontInfo::YOKO_MODE ||
        device_width <= 0 || device_height <= 0 ||
        screen_device_width <= 0 || screen_device_height <= 0) return;

    const double source_scale_x = static_cast<double>(screen_width) /
                                  std::max(1, screen_device_width);
    const double source_scale_y = static_cast<double>(screen_height) /
                                  std::max(1, screen_device_height);
    const auto clamp_source = [](int value, int low, int high) {
        return std::max(low, std::min(value, high));
    };
    const int visible_left = clamp_source(static_cast<int>(ceil(
        std::max(0, -render_view_rect.x) * source_scale_x)), 0, screen_width);
    const int visible_right = clamp_source(static_cast<int>(ceil(
        std::min(device_width - render_view_rect.x, screen_device_width) *
        source_scale_x)), 0, screen_width);
    const int visible_top = clamp_source(static_cast<int>(ceil(
        std::max(0, -render_view_rect.y) * source_scale_y)), 0, screen_height);
    const int visible_bottom = clamp_source(static_cast<int>(ceil(
        std::min(device_height - render_view_rect.y, screen_device_height) *
        source_scale_y)), 0, screen_height);

    if (crop_horizontal) {
        const int pitch = std::max(1, sentence_font.pitch_xy[0]);
        const int original_right = roc_sentence_top_x_original +
                                   roc_sentence_num_x_original * pitch;
        const int constrained_left = std::max(roc_sentence_top_x_original, visible_left);
        const int constrained_right = std::min(original_right, visible_right);
        sentence_font.top_xy[0] = constrained_left;
        sentence_font.num_xy[0] = std::max(1, (constrained_right - constrained_left) / pitch);
    }

    if (crop_vertical) {
        const int pitch = std::max(1, sentence_font.pitch_xy[1]);
        const int original_top = roc_sentence_top_y_original;
        const int original_bottom = original_top + roc_sentence_num_y_original * pitch;
        int constrained_top = std::max(original_top, visible_top);
        int constrained_bottom = std::min(original_bottom, visible_bottom);
        if (original_bottom > visible_bottom && original_top < visible_bottom) {
            constrained_top = std::max(visible_top,
                visible_bottom - roc_sentence_num_y_original * pitch);
            constrained_bottom = visible_bottom;
        }
        sentence_font.top_xy[1] = constrained_top;
        sentence_font.num_xy[1] = std::max(1, (constrained_bottom - constrained_top) / pitch);
    }
}

void ONScripter::drawRocVirtualMouseCursor""",
        "sentence safe-area hooks",
    )

    source = replace_once(
        source,
        r"    if \(roc_aspect && strcmp\(roc_aspect, \"fill-height\"\) == 0\)\n"
        r"        roc_aspect_mode = ROC_ASPECT_FILL_HEIGHT;",
        """    if (roc_aspect && strcmp(roc_aspect, "fill-height") == 0)
        roc_aspect_mode = ROC_ASPECT_FILL_HEIGHT;
    else if (roc_aspect &&
             (strcmp(roc_aspect, "fill-width") == 0 ||
              strcmp(roc_aspect, "fit-width") == 0))
        roc_aspect_mode = ROC_ASPECT_FILL_WIDTH;""",
        "aspect mode parsing",
    )
    return source


def patch_header(source: str) -> str:
    return replace_once(
        source,
        r"enum RocAspectMode \{ ROC_ASPECT_CONTAIN, ROC_ASPECT_FILL_HEIGHT \};",
        "enum RocAspectMode { ROC_ASPECT_CONTAIN, ROC_ASPECT_FILL_HEIGHT, ROC_ASPECT_FILL_WIDTH };",
        "aspect enum",
    ).replace(
        "    int roc_sentence_num_x_original;\n",
        "    int roc_sentence_num_x_original;\n"
        "    int roc_sentence_top_y_original;\n"
        "    int roc_sentence_num_y_original;\n",
        1,
    )


def main() -> int:
    if len(sys.argv) != 5:
        raise SystemExit("usage: patch_onsyuri.py CPP_IN CPP_OUT HEADER_IN HEADER_OUT")
    cpp_in, cpp_out, header_in, header_out = map(Path, sys.argv[1:])
    cpp_out.parent.mkdir(parents=True, exist_ok=True)
    cpp_out.write_text(patch_cpp(cpp_in.read_text(encoding="utf-8")), encoding="utf-8")
    header_out.write_text(patch_header(header_in.read_text(encoding="utf-8")), encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
