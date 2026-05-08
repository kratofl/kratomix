#!/usr/bin/env python3

from __future__ import annotations

import argparse
import re
import shutil
from pathlib import Path

SLUG_RE = re.compile(r"^[a-z0-9]+(?:-[a-z0-9]+)*$")
CODE_RE = re.compile(r"^[A-Za-z0-9]{4}$")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Create a new Kratomix plugin from the shared template.")
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1], help="Monorepo root")
    parser.add_argument("--template", default="effect-plugin", help="Template name under templates/")
    parser.add_argument("--slug", required=True, help="Plugin directory slug, for example tape-bloom")
    parser.add_argument("--name", required=True, help="Plugin product name, for example Kratomix Tape Bloom")
    parser.add_argument("--code", required=True, help="Four-character plugin code")
    parser.add_argument("--bundle", help="Bundle identifier, defaults to com.kratomix.<slug>")
    return parser.parse_args()


def validate_inputs(slug: str, code: str) -> None:
    if not SLUG_RE.fullmatch(slug):
        raise SystemExit(f"Invalid slug '{slug}'. Use lowercase letters, numbers, and single hyphens.")

    if not CODE_RE.fullmatch(code):
        raise SystemExit(f"Invalid CODE '{code}'. Use exactly 4 ASCII letters or digits.")


def slug_to_target_name(slug: str) -> str:
    parts = slug.split("-")
    return "Kratomix" + "".join(part.capitalize() for part in parts)


def render_text(text: str, replacements: dict[str, str]) -> str:
    for placeholder, value in replacements.items():
        text = text.replace(f"__{placeholder}__", value)
    return text


def copy_template(template_dir: Path, destination_dir: Path, replacements: dict[str, str]) -> None:
    for source_path in sorted(template_dir.rglob("*")):
        relative_path = source_path.relative_to(template_dir)
        output_parts = [part[:-9] if part.endswith(".template") else part for part in relative_path.parts]
        output_path = destination_dir.joinpath(*output_parts)

        if source_path.is_dir():
            output_path.mkdir(parents=True, exist_ok=True)
            continue

        output_path.parent.mkdir(parents=True, exist_ok=True)
        rendered = render_text(source_path.read_text(encoding="utf-8"), replacements)
        output_path.write_text(rendered, encoding="utf-8")


def main() -> None:
    args = parse_args()
    root = args.root.resolve()
    template_dir = root / "templates" / args.template
    if not template_dir.is_dir():
        raise SystemExit(f"Unknown template '{args.template}' at {template_dir}")

    validate_inputs(args.slug, args.code)

    destination_dir = root / "plugins" / args.slug
    if destination_dir.exists():
        raise SystemExit(f"Destination already exists: {destination_dir}")

    bundle_id = args.bundle or f"com.kratomix.{args.slug}"
    replacements = {
        "SLUG": args.slug,
        "PRODUCT_NAME": args.name,
        "PLUGIN_CODE": args.code,
        "BUNDLE_ID": bundle_id,
        "TARGET_NAME": slug_to_target_name(args.slug),
    }

    destination_dir.parent.mkdir(parents=True, exist_ok=True)
    copy_template(template_dir, destination_dir, replacements)

    print(destination_dir)


if __name__ == "__main__":
    main()
