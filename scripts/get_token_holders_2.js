import fetch from 'node-fetch';   // remove if on Node 18+ w/ built-in fetch
import fs from 'fs';
import pLimit from 'p-limit';

/**
 * Configuration
 */
const ENDPOINT_V1 = 'https://mainnet.telos.net/v1/chain/get_table_by_scope';
const ENDPOINT_V2_TOKENS = 'https://mainnet.telos.net/v2/state/get_tokens';

const CONTRACT = 'token.seeds';  // The contract to scan
const TABLE = 'accounts';        // The table in that contract
const PAGE_LIMIT = 1000;         // How many scopes to pull per request batch

// Files where we save progress
const SCOPES_FILE = 'scopes_progress.json';
const RESULTS_FILE = 'results_progress.json';

// Limit parallel fetches to 5
const limit = pLimit(5);

/**
 * Main entry point
 */
async function main() {
  try {
    console.log('Starting reentrant SEEDS balance fetch with parallel requests...');

    // 1) Load existing scope progress
    let { allScopes, lastScope, moreData } = loadScopesProgress();
    let resultsMap = loadResultsProgress(); // { accountName -> balance }

    console.log(`Loaded ${allScopes.length} scopes from disk.`);
    console.log(`Last scope fetched: "${lastScope}", moreData: ${moreData}`);

    // 2) If there's more scopes to discover, fetch them
    if (moreData) {
      console.log('Fetching additional scopes from chain...');
      const newlyFoundScopes = await getAllScopesIncremental(allScopes, lastScope);

      if (newlyFoundScopes.length > 0) {
        console.log(`Discovered ${newlyFoundScopes.length} new scopes.`);
      } else {
        console.log('No new scopes discovered (possibly end of table).');
      }

      // Save updated scopes to file
      saveScopesProgress(allScopes);
      console.log(`Now have ${allScopes.length} total scopes in memory.`);
    } else {
      console.log('No more scopes to fetch; we reached the end previously.');
    }

    // 3) Build a list of *unfetched* scopes (accounts)
    const scopesToFetch = allScopes
      .filter((scopeObj) => !(scopeObj.scope in resultsMap)); // skip accounts we already have

    console.log(`Need to fetch balances for ${scopesToFetch.length} accounts.`);

    // 4) Fetch balances in parallel (limit 5 at a time)
    let completed = 0;
    const tasks = scopesToFetch.map((scopeObj) => {
      return limit(async () => {
        const balance = await fetchSeedsBalance(scopeObj.scope);
        resultsMap[scopeObj.scope] = balance;
        completed++;

        // Periodically save partial results
        if (completed % 50 === 0) {
          console.log(`Fetched balances for ${completed} new accounts; saving partial progress...`);
          saveResultsProgress(resultsMap);
        }
      });
    });

    // Wait for all fetch tasks to finish
    await Promise.all(tasks);

    // Final save of results
    saveResultsProgress(resultsMap);

    // 5) Sort and display top 10
    const finalArray = Object.entries(resultsMap)
      .map(([account, balance]) => ({ account, balance }));
    finalArray.sort((a, b) => b.balance - a.balance);

    console.log('Top 10 holders by SEEDS balance:');
    console.table(finalArray);
    
    // Now save to CSV
    const csvHeader = 'balance,account\n';
    const csvRows = finalArray
    .map(item => `${item.balance},${item.account}`)
    .join('\n');

    fs.writeFileSync('seeds_top_holders.csv', csvHeader + csvRows, 'utf-8');

    console.log('CSV file saved: seeds_top_holders.csv');
    console.log('Done!');
  } catch (err) {
    console.error('Error in main:', err);
  }
}

/**
 * Fetch *new* scopes from the chain, starting at lastScope (lower_bound).
 * Append them to `allScopes` in memory. Update `scopes_progress.json`.
 */
async function getAllScopesIncremental(allScopes, lastScopeValue) {
  let newScopes = [];
  let lower_bound = lastScopeValue || '';
  let keepGoing = true;

  while (keepGoing) {
    const body = {
      code: CONTRACT,
      table: TABLE,
      lower_bound,
      limit: PAGE_LIMIT,
    };

    const response = await fetch(ENDPOINT_V1, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(body),
    });
    if (!response.ok) {
      throw new Error(`Error from get_table_by_scope: ${response.statusText}`);
    }

    const result = await response.json();

    // If no rows returned, might be end of data
    if (result.rows.length === 0) {
      updateScopesProgress(allScopes, lower_bound, false);
      keepGoing = false;
      break;
    }

    // Add newly discovered scopes
    for (const row of result.rows) {
      if (!allScopes.find((s) => s.scope === row.scope)) {
        allScopes.push(row);
        newScopes.push(row);
      }
    }

    if (!result.more) {
      // No more data
      updateScopesProgress(allScopes, result.rows[result.rows.length - 1].scope, false);
      keepGoing = false;
    } else {
      // There's more data
      const lastRow = result.rows[result.rows.length - 1];
      lower_bound = lastRow.scope;
      updateScopesProgress(allScopes, lower_bound, true);
    }
  }

  return newScopes;
}

/**
 * Fetch the SEEDS balance for a given account, by hitting /v2/state/get_tokens
 * and filtering for the `token.seeds` contract, `SEEDS` symbol.
 */
async function fetchSeedsBalance(account) {
  try {
    const url = `${ENDPOINT_V2_TOKENS}?account=${account}`;
    const response = await fetch(url);
    if (!response.ok) {
      throw new Error(`Error fetching tokens for account ${account}: ${response.statusText}`);
    }
    const data = await response.json();

    const tokens = data.tokens || [];
    const seedsToken = tokens.find((t) => t.contract === 'token.seeds' && t.symbol === 'SEEDS');
    return seedsToken ? parseFloat(seedsToken.amount) : 0.0;
  } catch (err) {
    console.error(`Error in fetchSeedsBalance for account ${account}:`, err);
    // Return 0 on error
    return 0.0;
  }
}

/**
 * READ or INIT scopes progress from disk.
 * Returns { allScopes, lastScope, moreData }.
 */
function loadScopesProgress() {
  if (!fs.existsSync(SCOPES_FILE)) {
    return {
      allScopes: [],
      lastScope: '',
      moreData: true,
    };
  }
  try {
    const content = fs.readFileSync(SCOPES_FILE, 'utf-8');
    const parsed = JSON.parse(content);
    return {
      allScopes: parsed.allScopes || [],
      lastScope: parsed.lastScope || '',
      moreData: typeof parsed.moreData === 'boolean' ? parsed.moreData : true,
    };
  } catch (err) {
    console.error('Error reading scopes_progress.json:', err);
    return {
      allScopes: [],
      lastScope: '',
      moreData: true,
    };
  }
}

/**
 * SAVE scopes progress to disk.
 */
function saveScopesProgress(allScopes) {
  const lastScope = allScopes.length
    ? allScopes[allScopes.length - 1].scope
    : '';
  const moreData = (allScopes.length > 0);

  const out = {
    allScopes,
    lastScope,
    moreData,
  };
  fs.writeFileSync(SCOPES_FILE, JSON.stringify(out, null, 2), 'utf-8');
}

/**
 * Update scopes progress with new info and write to disk.
 */
function updateScopesProgress(allScopes, lastScope, moreData) {
  const out = {
    allScopes,
    lastScope,
    moreData,
  };
  fs.writeFileSync(SCOPES_FILE, JSON.stringify(out, null, 2), 'utf-8');
}

/**
 * READ or INIT results progress from disk.
 * Return as an object { [accountName]: numberBalance }
 */
function loadResultsProgress() {
  if (!fs.existsSync(RESULTS_FILE)) {
    return {};
  }
  try {
    const content = fs.readFileSync(RESULTS_FILE, 'utf-8');
    const parsed = JSON.parse(content);
    return parsed; // e.g. { "alice": 123.45, "bob": 0.0 }
  } catch (err) {
    console.error('Error reading results_progress.json:', err);
    return {};
  }
}

/**
 * SAVE results progress to disk.
 */
function saveResultsProgress(resultsMap) {
  fs.writeFileSync(RESULTS_FILE, JSON.stringify(resultsMap, null, 2), 'utf-8');
}

// Run main
main();