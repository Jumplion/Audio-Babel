"""
Manim explainer for Audio-Babel's index generation/decoding algorithm.

Renders with:
    manim -pqh index_algorithm.py IndexAlgorithm     # 1080p60, preview
    manim -pql index_algorithm.py IndexAlgorithm     # 480p15, fast draft

See animations/README.md for setup instructions (this needs a Python
virtualenv with `manim` installed; no LaTeX install is required since the
scene only uses Pango-rendered Text/Code mobjects).
"""

from manim import *

MONO = "DejaVu Sans Mono"

SAMPLE_COLOR = BLUE_D
DIGIT_COLOR = TEAL_D
GOOD_COLOR = GREEN
BAD_COLOR = RED
INDEX_COLOR = GOLD_D


MAX_WIDTH = 12.5


def fit(mobj, max_width=MAX_WIDTH):
    """Defensively shrink a mobject so it never runs off the frame edge.

    Pango/Manim text width does not scale linearly with font_size (glyph
    metrics get rounded at certain sizes), so overflow is only caught
    reliably by measuring the actual rendered width, not by picking a
    font_size analytically.
    """
    if mobj.width > max_width:
        mobj.scale_to_fit_width(max_width)
    return mobj


def boxes_for(values, box_color=SAMPLE_COLOR, side=1.0, font_size=40):
    """A row of numbered boxes representing a sequence of digits/samples."""
    group = VGroup()
    for v in values:
        sq = Square(side_length=side, color=box_color, fill_opacity=0.15)
        label = Text(str(v), font=MONO, font_size=font_size)
        label.move_to(sq.get_center())
        cell = VGroup(sq, label)
        group.add(cell)
    group.arrange(RIGHT, buff=0.15)
    return group


def title_card(main_text, sub_text=None, main_size=56, sub_size=32):
    main = fit(Text(main_text, font=MONO, weight=BOLD, font_size=main_size))
    if sub_text is None:
        return VGroup(main), main
    sub = fit(Text(sub_text, font=MONO, font_size=sub_size, color=GRAY_B))
    sub.next_to(main, DOWN, buff=0.4)
    return VGroup(main, sub), main


class IndexAlgorithm(Scene):
    def construct(self):
        self.scene_title()
        self.scene_problem_setup()
        self.scene_naive_collision()
        self.scene_bijective_fix()
        self.scene_encode_walkthrough()
        self.scene_decode_walkthrough()
        self.scene_bands()
        self.scene_scale_up()
        self.scene_base64()
        self.scene_recap()

    # ------------------------------------------------------------------
    def clear_scene(self, extra_wait=0.2):
        self.play(FadeOut(*self.mobjects))
        self.wait(extra_wait)

    # ------------------------------------------------------------------
    def scene_title(self):
        group, _ = title_card(
            "Audio → Integer → Audio",
            "How Audio-Babel's index algorithm works",
        )
        group.move_to(ORIGIN)
        self.play(FadeIn(group, shift=UP * 0.3))
        self.wait(1.5)
        self.clear_scene()

    # ------------------------------------------------------------------
    def scene_problem_setup(self):
        heading = fit(Text("An audio clip is just a list of numbers", font=MONO, font_size=40))
        heading.to_edge(UP)
        self.play(FadeIn(heading))

        samples = [3, 0, 12, 5]
        row = boxes_for(samples)
        row.move_to(ORIGIN)
        caption = fit(Text("PCM samples, each in 0 .. B-1", font=MONO, font_size=28, color=GRAY_B))
        caption.next_to(row, DOWN, buff=0.5)

        self.play(LaggedStart(*[FadeIn(c, shift=DOWN * 0.2) for c in row], lag_ratio=0.15))
        self.play(FadeIn(caption))
        self.wait(0.5)

        goal = fit(
            Text(
                "Goal: fold this list into ONE big integer,\nand back out — with zero loss.",
                font=MONO,
                font_size=28,
            )
        )
        goal.next_to(caption, DOWN, buff=0.6)
        self.play(FadeIn(goal))
        self.wait(2)
        self.clear_scene()

    # ------------------------------------------------------------------
    def scene_naive_collision(self):
        heading = fit(Text("The obvious approach breaks", font=MONO, font_size=40))
        heading.to_edge(UP)
        self.play(FadeIn(heading))

        formula = fit(
            Code(
                code_string="n = n * B + v      # ordinary base-B digits, v in 0..B-1",
                language="python",
                formatter_style="monokai",
            ).scale(0.9)
        )
        formula.next_to(heading, DOWN, buff=0.6)
        self.play(FadeIn(formula))
        self.wait(1)

        # Two different sample sequences, same resulting integer.
        row_a = boxes_for([3])
        label_a = Text("samples = [3]", font=MONO, font_size=28)
        group_a = VGroup(label_a, row_a).arrange(DOWN, buff=0.3)

        row_b = boxes_for([0, 3])
        label_b = Text("samples = [0, 3]", font=MONO, font_size=28)
        group_b = VGroup(label_b, row_b).arrange(DOWN, buff=0.3)

        pair = VGroup(group_a, group_b).arrange(RIGHT, buff=2.0)
        pair.next_to(formula, DOWN, buff=0.8)
        self.play(FadeIn(pair))
        self.wait(0.5)

        n_a = Text("n = 3", font=MONO, font_size=32, color=INDEX_COLOR)
        n_a.next_to(group_a, DOWN, buff=0.4)
        n_b = Text("n = 3", font=MONO, font_size=32, color=INDEX_COLOR)
        n_b.next_to(group_b, DOWN, buff=0.4)
        self.play(FadeIn(n_a), FadeIn(n_b))
        self.wait(0.5)

        eq = fit(Text("same integer, different audio!", font=MONO, font_size=30, color=BAD_COLOR))
        eq.next_to(pair, DOWN, buff=1.1)
        box = SurroundingRectangle(VGroup(n_a, n_b), color=BAD_COLOR)
        self.play(Create(box), FadeIn(eq))
        self.wait(2)
        self.clear_scene()

    # ------------------------------------------------------------------
    def scene_bijective_fix(self):
        heading = fit(Text("The fix: bijective numeration", font=MONO, font_size=40))
        heading.to_edge(UP)
        self.play(FadeIn(heading))

        rule = fit(
            Code(
                code_string="n = n * B + (v + 1)     # digit = value + 1, so digits are 1..B, never 0",
                language="python",
                formatter_style="monokai",
            ).scale(0.85)
        )
        rule.next_to(heading, DOWN, buff=0.6)
        self.play(FadeIn(rule))
        self.wait(1)

        row_a = boxes_for([3])
        label_a = Text("samples = [3]", font=MONO, font_size=28)
        group_a = VGroup(label_a, row_a).arrange(DOWN, buff=0.3)

        row_b = boxes_for([0, 3])
        label_b = Text("samples = [0, 3]", font=MONO, font_size=28)
        group_b = VGroup(label_b, row_b).arrange(DOWN, buff=0.3)

        pair = VGroup(group_a, group_b).arrange(RIGHT, buff=2.0)
        pair.next_to(rule, DOWN, buff=0.8)
        self.play(FadeIn(pair))
        self.wait(0.3)

        n_a = Text("digit 4  →  n = 4", font=MONO, font_size=30, color=DIGIT_COLOR)
        n_a.next_to(group_a, DOWN, buff=0.4)
        n_b = Text("digits 1,4  →  n = 14", font=MONO, font_size=30, color=DIGIT_COLOR)
        n_b.next_to(group_b, DOWN, buff=0.4)
        self.play(FadeIn(n_a), FadeIn(n_b))
        self.wait(0.5)

        ok = fit(Text("different integers — no collision", font=MONO, font_size=30, color=GOOD_COLOR))
        ok.next_to(pair, DOWN, buff=1.1)
        box = SurroundingRectangle(VGroup(n_a, n_b), color=GOOD_COLOR)
        self.play(Create(box), FadeIn(ok))
        self.wait(2)
        self.clear_scene()

    # ------------------------------------------------------------------
    def scene_encode_walkthrough(self):
        heading = fit(Text("Encoding, step by step (B = 10)", font=MONO, font_size=40))
        heading.to_edge(UP)
        self.play(FadeIn(heading))

        samples = [2, 0, 5]
        row = boxes_for(samples)
        row.next_to(heading, DOWN, buff=0.6)
        self.play(FadeIn(row))

        n_text = Text("n = 0", font=MONO, font_size=36, color=INDEX_COLOR)
        n_text.next_to(row, DOWN, buff=0.7)
        self.play(FadeIn(n_text))
        self.wait(0.5)

        n = 0
        for i, v in enumerate(samples):
            highlight = SurroundingRectangle(row[i], color=YELLOW)
            self.play(Create(highlight))

            digit = v + 1
            n = n * 10 + digit
            step = fit(
                Text(
                    f"v={v}  →  digit={digit}   n = {n // 10 if i > 0 else 0}×10 + {digit} = {n}",
                    font=MONO,
                    font_size=28,
                )
            )
            step.next_to(n_text, DOWN, buff=0.4)
            new_n = Text(f"n = {n}", font=MONO, font_size=36, color=INDEX_COLOR)
            new_n.move_to(n_text)

            self.play(FadeIn(step))
            self.wait(0.6)
            self.play(Transform(n_text, new_n), FadeOut(step), FadeOut(highlight))
            self.wait(0.3)

        result = Text(f"index = {n}", font=MONO, font_size=40, color=GOOD_COLOR)
        result.next_to(n_text, DOWN, buff=0.6)
        self.play(FadeIn(result, shift=UP * 0.2))
        self.wait(2)
        self.clear_scene()

    # ------------------------------------------------------------------
    def scene_decode_walkthrough(self):
        heading = fit(Text("Decoding: peel the digits back off", font=MONO, font_size=40))
        heading.to_edge(UP)
        self.play(FadeIn(heading))

        algo = fit(
            Code(
                code_string=(
                    "while n > 0:\n"
                    "    n -= 1\n"
                    "    v = n mod 10   # emit v\n"
                    "    n = n // 10\n"
                    "# then reverse the emitted values"
                ),
                language="python",
                formatter_style="monokai",
            ).scale(0.75)
        )
        algo.next_to(heading, DOWN, buff=0.5)
        self.play(FadeIn(algo))
        self.wait(1)

        n = 316
        n_text = Text(f"n = {n}", font=MONO, font_size=34, color=INDEX_COLOR)
        n_text.next_to(algo, DOWN, buff=0.6)
        self.play(FadeIn(n_text))

        emitted = []
        emitted_group = VGroup()
        emitted_group.next_to(n_text, DOWN, buff=0.6)
        self.add(emitted_group)

        while n > 0:
            n -= 1
            v = n % 10
            n = n // 10
            emitted.append(v)

            step = fit(Text(f"emit {v}   (n → {n})", font=MONO, font_size=28, color=DIGIT_COLOR))
            step.next_to(n_text, DOWN, buff=0.4)
            self.play(FadeIn(step))

            new_n_text = Text(f"n = {n}", font=MONO, font_size=34, color=INDEX_COLOR)
            new_n_text.move_to(n_text)

            new_box = boxes_for([v], box_color=DIGIT_COLOR, side=0.8, font_size=32)
            new_emitted_group = VGroup(*emitted_group.copy(), new_box)
            new_emitted_group.arrange(RIGHT, buff=0.15)
            new_emitted_group.next_to(n_text, DOWN, buff=1.1)

            self.play(
                Transform(n_text, new_n_text),
                Transform(emitted_group, new_emitted_group),
                FadeOut(step),
            )
            self.wait(0.3)

        # Free up vertical space: the code and running-n readout have done
        # their job, so drop them before laying out the final results.
        self.play(FadeOut(algo), FadeOut(n_text))
        self.play(emitted_group.animate.next_to(heading, DOWN, buff=0.8))

        emitted_label = fit(Text("emitted (in emit order)", font=MONO, font_size=24, color=GRAY_B))
        emitted_label.next_to(emitted_group, DOWN, buff=0.3)
        self.play(FadeIn(emitted_label))
        self.wait(0.8)

        reversed_vals = list(reversed(emitted))
        final_row = boxes_for(reversed_vals, box_color=SAMPLE_COLOR)
        final_row.next_to(emitted_label, DOWN, buff=0.6)
        final_label = fit(Text("reversed → original samples", font=MONO, font_size=28, color=GOOD_COLOR))
        final_label.next_to(final_row, DOWN, buff=0.3)

        self.play(FadeIn(final_row, shift=UP * 0.2))
        self.play(FadeIn(final_label))
        self.wait(2)
        self.clear_scene()

    # ------------------------------------------------------------------
    def scene_bands(self):
        heading = fit(Text("Why length never needs to be stored", font=MONO, font_size=38))
        heading.to_edge(UP)
        self.play(FadeIn(heading))

        sub = fit(
            Text(
                "Every sample count L owns its own exclusive range of integers",
                font=MONO,
                font_size=26,
                color=GRAY_B,
            )
        )
        sub.next_to(heading, DOWN, buff=0.3)
        self.play(FadeIn(sub))

        # S_L = repunit in base 10: 0, 1, 11, 111, 1111 for L = 0..4.
        # Bands grow 10x each step, so we draw fixed-width labeled blocks
        # rather than a true-to-scale number line.
        blocks = VGroup()
        block_texts = [
            ("L=0", "{0}", GRAY),
            ("L=1", "[1 .. 10]", BLUE_D),
            ("L=2", "[11 .. 110]", TEAL_D),
            ("L=3", "[111 .. 1110]", GREEN_D),
        ]
        for name, rng, color in block_texts:
            rect = Rectangle(width=2.6, height=1.0, color=color, fill_opacity=0.2)
            t1 = Text(name, font=MONO, font_size=26, color=color)
            t2 = Text(rng, font=MONO, font_size=22)
            txt = VGroup(t1, t2).arrange(DOWN, buff=0.1)
            txt.move_to(rect.get_center())
            blocks.add(VGroup(rect, txt))
        blocks.arrange(RIGHT, buff=0.3)
        blocks.next_to(sub, DOWN, buff=1.0)

        self.play(LaggedStart(*[FadeIn(b) for b in blocks], lag_ratio=0.2))
        self.wait(0.5)

        note = fit(
            Text(
                "S_L = 1, 11, 111, ...  (repunit) marks where each band starts",
                font=MONO,
                font_size=24,
                color=GRAY_B,
            )
        )
        note.next_to(blocks, DOWN, buff=0.6)
        self.play(FadeIn(note))
        self.wait(1)

        pointer_val = fit(
            Text("n = 316  →  falls in the L=3 band  →  decodes to 3 samples", font=MONO, font_size=26, color=INDEX_COLOR)
        )
        pointer_val.next_to(note, DOWN, buff=0.5)
        highlight_block = SurroundingRectangle(blocks[3], color=INDEX_COLOR, buff=0.05)
        self.play(FadeIn(pointer_val), Create(highlight_block))
        self.wait(2.5)
        self.clear_scene()

    # ------------------------------------------------------------------
    def scene_scale_up(self):
        heading = fit(Text("The real algorithm: same idea, bigger numbers", font=MONO, font_size=34))
        heading.to_edge(UP)
        self.play(FadeIn(heading))

        pts = fit(
            VGroup(
                Text("B = 65536   (16-bit PCM samples, 0..65535)", font=MONO, font_size=28),
                Text("A 3-minute clip → ~8 million samples", font=MONO, font_size=28),
                Text("index n becomes a huge integer (millions of digits)", font=MONO, font_size=28),
            ).arrange(DOWN, buff=0.5, aligned_edge=LEFT)
        )
        pts.next_to(heading, DOWN, buff=0.8)
        for p in pts:
            self.play(FadeIn(p, shift=RIGHT * 0.2))
            self.wait(0.4)
        self.wait(0.5)

        formula = fit(
            Code(
                code_string="n = V + S_L    # V = samples read as one base-B number, S_L = repunit",
                language="python",
                formatter_style="monokai",
            ).scale(0.8)
        )
        formula.next_to(pts, DOWN, buff=0.8)
        self.play(FadeIn(formula))
        self.wait(0.5)

        note = fit(
            Text(
                "One big-integer addition instead of an O(L²) digit-by-digit loop",
                font=MONO,
                font_size=24,
                color=GRAY_B,
            )
        )
        note.next_to(formula, DOWN, buff=0.4)
        self.play(FadeIn(note))
        self.wait(2.5)
        self.clear_scene()

    # ------------------------------------------------------------------
    def scene_base64(self):
        heading = fit(Text("Same trick again: integer → short string", font=MONO, font_size=36))
        heading.to_edge(UP)
        self.play(FadeIn(heading))

        alphabet = fit(
            Text(
                "A..Z a..z 0..9 - _   (64 URL-safe symbols)",
                font=MONO,
                font_size=30,
            )
        )
        alphabet.next_to(heading, DOWN, buff=0.7)
        self.play(FadeIn(alphabet))
        self.wait(0.5)

        rule = fit(
            Code(
                code_string="n -= 1; emit ALPHA[n mod 64]; n //= 64      # bijective base-64, digit = value+1",
                language="python",
                formatter_style="monokai",
            ).scale(0.75)
        )
        rule.next_to(alphabet, DOWN, buff=0.7)
        self.play(FadeIn(rule))
        self.wait(0.8)

        bullets = fit(
            VGroup(
                Text("• identical bijective-numeration idea, base 64 instead of base 65536", font=MONO, font_size=26),
                Text("• empty string ↔ 0, and every alphabet-valid string decodes to something", font=MONO, font_size=26),
            ).arrange(DOWN, buff=0.4, aligned_edge=LEFT)
        )
        bullets.next_to(rule, DOWN, buff=0.7)
        self.play(FadeIn(bullets))
        self.wait(2.5)
        self.clear_scene()

    # ------------------------------------------------------------------
    def scene_recap(self):
        heading = fit(Text("Recap", font=MONO, weight=BOLD, font_size=44))
        heading.to_edge(UP)
        self.play(FadeIn(heading))

        bullets = fit(
            VGroup(
                Text("1. digit = sample value + 1  —  never 0, so nothing vanishes", font=MONO, font_size=28),
                Text("2. each sample count L owns its own range (band) of integers", font=MONO, font_size=28),
                Text("3. that band tells you L back, with no length stored anywhere", font=MONO, font_size=28),
                Text("4. the same bijection encodes the big integer as a base-64 string", font=MONO, font_size=28),
                Text(
                    "5. result: a TRUE bijection — no gaps, no collisions, nothing rejected",
                    font=MONO,
                    font_size=28,
                    color=GOOD_COLOR,
                ),
            ).arrange(DOWN, buff=0.4, aligned_edge=LEFT)
        )
        bullets.next_to(heading, DOWN, buff=0.8)
        for b in bullets:
            self.play(FadeIn(b, shift=RIGHT * 0.2))
            self.wait(0.4)
        self.wait(2.5)
        self.clear_scene()

        outro, _ = title_card("Audio-Babel", "docs/INDEX_FORMAT.md")
        outro.move_to(ORIGIN)
        self.play(FadeIn(outro))
        self.wait(2)
