#!/usr/bin/env node
// Compares a fresh performance_benchmarks JSON run against the committed
// baseline (see cpp/perf/results-schema.md for the shared schema) and
// reports per-benchmark regressions within a tolerance.
//
// Usage:
//   node tools/node/compare-benchmarks.mjs [options]
//
// Options:
//   --current <path>      Fresh run JSON (default: build/performance_results.json)
//   --baseline <path>     Baseline JSON (default: cpp/perf/baseline.json)
//   --tolerance <percent> Override baseline's tolerancePercent
//   --format <fmt>        text | markdown | json (default: text)
//   --summary-out <path>  Also append the formatted report to this file
//
// Exit codes:
//   0  no regressions
//   1  one or more regressions found
//   2  usage error or unreadable/unparseable input file

import { readFileSync, appendFileSync } from "node:fs";
import process from "node:process";

const HIGH_VARIANCE_RATIO = 0.25; // stddevMs / medianMs above this is noisy
const NOTABLE_IMPROVEMENT_PERCENT = 10; // faster than this is called out

function parseArgs(argv) {
    const args = {
        current: "build/performance_results.json",
        baseline: "cpp/perf/baseline.json",
        tolerance: undefined,
        format: "text",
        summaryOut: undefined,
    };
    for (let i = 0; i < argv.length; i++) {
        const arg = argv[i];
        const next = () => argv[++i];
        switch (arg) {
            case "--current":
                args.current = next();
                break;
            case "--baseline":
                args.baseline = next();
                break;
            case "--tolerance":
                args.tolerance = Number(next());
                break;
            case "--format":
                args.format = next();
                break;
            case "--summary-out":
                args.summaryOut = next();
                break;
            default:
                throw new UsageError(`Unknown argument: ${arg}`);
        }
    }
    if (!["text", "markdown", "json"].includes(args.format)) {
        throw new UsageError(`--format must be text, markdown, or json (got "${args.format}")`);
    }
    if (args.tolerance !== undefined && !Number.isFinite(args.tolerance)) {
        throw new UsageError("--tolerance must be a number");
    }
    return args;
}

class UsageError extends Error {}

function loadJson(path, label) {
    let text;
    try {
        text = readFileSync(path, "utf8");
    } catch (err) {
        throw new IoError(`Cannot read ${label} file "${path}": ${err.message}`);
    }
    try {
        return JSON.parse(text);
    } catch (err) {
        throw new IoError(`Cannot parse ${label} file "${path}" as JSON: ${err.message}`);
    }
}

class IoError extends Error {}

function compare(current, baseline, tolerancePercent) {
    const baselineByName = new Map(baseline.benchmarks.map((b) => [b.name, b]));
    const currentByName = new Map(current.benchmarks.map((b) => [b.name, b]));
    const allNames = new Set([...baselineByName.keys(), ...currentByName.keys()]);

    const rows = [];
    for (const name of allNames) {
        const cur = currentByName.get(name);
        const base = baselineByName.get(name);

        if (cur && base) {
            const deltaPercent = ((cur.medianMs - base.medianMs) / base.medianMs) * 100;
            const isRegression = cur.medianMs > base.medianMs * (1 + tolerancePercent / 100);
            const isImprovement = deltaPercent <= -NOTABLE_IMPROVEMENT_PERCENT;
            const isHighVariance = cur.medianMs > 0 && cur.stddevMs / cur.medianMs > HIGH_VARIANCE_RATIO;
            let status = "OK";
            if (isRegression) status = "REGRESSION";
            else if (isImprovement) status = "IMPROVED";
            rows.push({
                name,
                category: cur.category ?? base.category,
                baselineMs: base.medianMs,
                currentMs: cur.medianMs,
                deltaPercent,
                status,
                highVariance: isHighVariance,
            });
        } else if (cur && !base) {
            rows.push({
                name,
                category: cur.category,
                baselineMs: null,
                currentMs: cur.medianMs,
                deltaPercent: null,
                status: "NEW",
                highVariance: false,
            });
        } else {
            rows.push({
                name,
                category: base.category,
                baselineMs: base.medianMs,
                currentMs: null,
                deltaPercent: null,
                status: "MISSING",
                highVariance: false,
            });
        }
    }

    rows.sort((a, b) => a.category.localeCompare(b.category) || a.name.localeCompare(b.name));
    const regressionCount = rows.filter((r) => r.status === "REGRESSION").length;
    return { rows, regressionCount };
}

function groupByCategory(rows) {
    const groups = new Map();
    for (const row of rows) {
        if (!groups.has(row.category)) groups.set(row.category, []);
        groups.get(row.category).push(row);
    }
    return groups;
}

function fmtMs(value) {
    return value === null ? "-" : value.toFixed(3);
}

function fmtDelta(value) {
    if (value === null) return "-";
    const sign = value >= 0 ? "+" : "";
    return `${sign}${value.toFixed(1)}%`;
}

function buildHeader(current, baseline, tolerancePercent) {
    const platform = current.platform ?? {};
    return [
        `Commit: ${current.gitCommit ?? "unknown"}`,
        `Platform: ${platform.os ?? "?"} / ${platform.compiler ?? "?"} / ${platform.buildType ?? "?"} (${platform.archBits ?? "?"}-bit)`,
        `Baseline commit: ${baseline.gitCommit ?? "unknown"}`,
        `Tolerance: ${tolerancePercent}%`,
    ];
}

function renderText(current, baseline, tolerancePercent, rows, regressionCount) {
    const lines = [];
    lines.push("Performance Benchmark Comparison");
    lines.push("=================================");
    lines.push(...buildHeader(current, baseline, tolerancePercent));
    lines.push("");

    for (const [category, categoryRows] of groupByCategory(rows)) {
        lines.push(`-- ${category} --`);
        for (const row of categoryRows) {
            const variance = row.highVariance ? " (high variance)" : "";
            lines.push(
                `  [${row.status}] ${row.name}: baseline=${fmtMs(row.baselineMs)}ms current=${fmtMs(row.currentMs)}ms delta=${fmtDelta(row.deltaPercent)}${variance}`,
            );
        }
        lines.push("");
    }

    lines.push(
        regressionCount > 0
            ? `RESULT: ${regressionCount} regression(s) found (tolerance ${tolerancePercent}%).`
            : "RESULT: No regressions found.",
    );
    return lines.join("\n");
}

function renderMarkdown(current, baseline, tolerancePercent, rows, regressionCount) {
    const lines = [];
    if (regressionCount > 0) {
        lines.push("## ⚠️ Performance Regression Detected");
    } else {
        lines.push("## Performance Benchmark Comparison");
    }
    lines.push("");
    for (const headerLine of buildHeader(current, baseline, tolerancePercent)) {
        lines.push(`- ${headerLine}`);
    }
    lines.push("");

    for (const [category, categoryRows] of groupByCategory(rows)) {
        lines.push(`### ${category}`);
        lines.push("");
        lines.push("| Benchmark | Baseline (ms) | Current (ms) | Delta | Status |");
        lines.push("|---|---|---|---|---|");
        for (const row of categoryRows) {
            const statusLabel = row.highVariance ? `${row.status} ⚠️ high variance` : row.status;
            lines.push(
                `| ${row.name} | ${fmtMs(row.baselineMs)} | ${fmtMs(row.currentMs)} | ${fmtDelta(row.deltaPercent)} | ${statusLabel} |`,
            );
        }
        lines.push("");
    }

    lines.push(
        regressionCount > 0
            ? `**Result: ${regressionCount} regression(s) found (tolerance ${tolerancePercent}%).**`
            : "**Result: No regressions found.**",
    );
    return lines.join("\n");
}

function renderJson(current, baseline, tolerancePercent, rows, regressionCount) {
    return JSON.stringify(
        {
            gitCommit: current.gitCommit ?? "unknown",
            baselineCommit: baseline.gitCommit ?? "unknown",
            tolerancePercent,
            regressionCount,
            rows,
        },
        null,
        2,
    );
}

function main() {
    let args;
    try {
        args = parseArgs(process.argv.slice(2));
    } catch (err) {
        if (err instanceof UsageError) {
            console.error(`Usage error: ${err.message}`);
            return 2;
        }
        throw err;
    }

    let current;
    let baseline;
    try {
        current = loadJson(args.current, "current run");
        baseline = loadJson(args.baseline, "baseline");
    } catch (err) {
        if (err instanceof IoError) {
            console.error(err.message);
            return 2;
        }
        throw err;
    }

    const tolerancePercent = args.tolerance ?? baseline.tolerancePercent ?? 20;
    const { rows, regressionCount } = compare(current, baseline, tolerancePercent);

    const renderers = { text: renderText, markdown: renderMarkdown, json: renderJson };
    const output = renderers[args.format](current, baseline, tolerancePercent, rows, regressionCount);

    console.log(output);
    if (args.summaryOut) {
        appendFileSync(args.summaryOut, output + "\n");
    }

    return regressionCount > 0 ? 1 : 0;
}

process.exit(main());
