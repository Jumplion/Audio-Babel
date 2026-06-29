#!/usr/bin/env node
// Regenerates cpp/perf/baseline.json from a fresh performance_benchmarks run.
// Does NOT commit anything — review the printed diff, then `git add`/commit
// the updated baseline yourself with a justification for the change.
//
// Usage:
//   node tools/node/update-baseline.mjs [--build] [--current <path>] [--baseline <path>] [--tolerance <percent>]
//
// --build   Build and run performance_benchmarks in Release mode first
//           (via tools/bash/run_performance.sh), rather than reusing an
//           already-produced build/performance_results.json.

import { readFileSync, writeFileSync, existsSync } from "node:fs";
import { spawnSync } from "node:child_process";
import path from "node:path";
import { fileURLToPath } from "node:url";
import process from "node:process";

const REPO_ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..", "..");
const DEFAULT_TOLERANCE_PERCENT = 20;

function parseArgs(argv) {
    const args = {
        build: false,
        current: "build/performance_results.json",
        baseline: "cpp/perf/baseline.json",
        tolerance: undefined,
    };
    for (let i = 0; i < argv.length; i++) {
        const arg = argv[i];
        const next = () => argv[++i];
        switch (arg) {
            case "--build":
                args.build = true;
                break;
            case "--current":
                args.current = next();
                break;
            case "--baseline":
                args.baseline = next();
                break;
            case "--tolerance":
                args.tolerance = Number(next());
                break;
            default:
                console.error(`Usage error: unknown argument: ${arg}`);
                process.exit(2);
        }
    }
    return args;
}

function runBuild() {
    const script = path.join(REPO_ROOT, "tools", "bash", "run_performance.sh");
    console.log(`Building and running benchmarks via ${script} ...`);
    const result = spawnSync("bash", [script], { cwd: REPO_ROOT, stdio: "inherit" });
    if (result.status !== 0) {
        console.error("Benchmark build/run failed; aborting baseline update.");
        process.exit(2);
    }
}

function loadJson(absPath, label) {
    if (!existsSync(absPath)) {
        console.error(`Cannot find ${label} at "${absPath}".`);
        process.exit(2);
    }
    try {
        return JSON.parse(readFileSync(absPath, "utf8"));
    } catch (err) {
        console.error(`Cannot parse ${label} at "${absPath}": ${err.message}`);
        process.exit(2);
    }
}

function printDiff(oldBaseline, fresh) {
    const oldByName = new Map((oldBaseline?.benchmarks ?? []).map((b) => [b.name, b]));
    console.log("\nBaseline diff (old -> new medianMs):");
    console.log("======================================");
    for (const bench of fresh.benchmarks) {
        const old = oldByName.get(bench.name);
        if (!old) {
            console.log(`  [NEW]     ${bench.name}: ${bench.medianMs.toFixed(3)} ms`);
            continue;
        }
        const deltaPercent = ((bench.medianMs - old.medianMs) / old.medianMs) * 100;
        const sign = deltaPercent >= 0 ? "+" : "";
        console.log(
            `  ${old.medianMs.toFixed(3)} -> ${bench.medianMs.toFixed(3)} ms (${sign}${deltaPercent.toFixed(1)}%)  ${bench.name}`,
        );
    }
    for (const old of oldByName.values()) {
        if (!fresh.benchmarks.some((b) => b.name === old.name)) {
            console.log(`  [REMOVED] ${old.name}: was ${old.medianMs.toFixed(3)} ms`);
        }
    }
}

function main() {
    const args = parseArgs(process.argv.slice(2));
    const currentPath = path.resolve(REPO_ROOT, args.current);
    const baselinePath = path.resolve(REPO_ROOT, args.baseline);

    if (args.build) {
        runBuild();
    }

    const fresh = loadJson(currentPath, "fresh run JSON");
    const oldBaseline = existsSync(baselinePath) ? loadJson(baselinePath, "existing baseline") : null;

    printDiff(oldBaseline, fresh);

    const tolerancePercent = args.tolerance ?? oldBaseline?.tolerancePercent ?? DEFAULT_TOLERANCE_PERCENT;
    const newBaseline = { ...fresh, tolerancePercent };

    writeFileSync(baselinePath, JSON.stringify(newBaseline, null, 2) + "\n");

    console.log(`\nWrote new baseline to ${path.relative(REPO_ROOT, baselinePath)} (tolerancePercent=${tolerancePercent}).`);
    console.log("This was NOT committed. Review the diff above, then:");
    console.log(`  git add ${path.relative(REPO_ROOT, baselinePath)}`);
    console.log('  git commit -m "Update performance baseline: <reason>"');
}

main();
