/**
 * top_grant_receivers.js
 *
 * Fetch all `transfer` actions where:
 *   - account = gift.seeds
 *   - act.name = transfer
 *
 * We'll do date-based descending pagination from the newest to the oldest,
 * only stopping if we get 0 new actions in a response.
 * We accumulate amounts for "from=gift.seeds" to recipients.
 *
 * Produce two CSVs:
 *   1) grant_receivers.csv   (top recipients by total received)
 *   2) all_recipients.csv    (every transfer in ascending time order)
 */

import fetch from 'node-fetch'; // If on Node 18+, you can remove and use the built-in fetch
import fs from 'fs';

// Telos Hyperion endpoint
const HYPERION_ENDPOINT = 'https://mainnet.telos.net/v2/history/get_actions';

// We'll fetch up to 1000 actions per page
const LIMIT = 1000;

// CSV output files
const CSV_TOP = 'grant_receivers.csv';  // total amounts by recipient
const CSV_ALL = 'all_recipients.csv';   // all transfers, sorted by time

// Keep track of global_sequence values to avoid duplicates
const seenGlobalSequences = new Set();

async function main() {
  try {
    console.log('Fetching transfer actions (gift.seeds → recipients) with descending date pagination...');

    // 1) Get all actions
    const allActions = await fetchAllGiftSeedsTransfers();
    console.log(`\nFetched a total of ${allActions.length} unique transfer actions.`);

    // 2) We'll build:
    //    - recipientsMap for total amounts
    //    - allTransfers array for every individual transfer
    const recipientsMap = {};       // { accountName: totalAmount }
    const allTransfers = [];        // Each item: { account, received, timestamp }

    for (const action of allActions) {
      const { from, to, quantity } = action.act.data;
      if (from === 'gift.seeds' && quantity && quantity.endsWith(' SEEDS')) {
        // e.g. "50000.0000 SEEDS"
        const [amountStr] = quantity.split(' ');
        const amount = parseFloat(amountStr) || 0;

        // Tally into recipientsMap
        recipientsMap[to] = (recipientsMap[to] || 0) + amount;

        // Also add to allTransfers array
        const tstamp = action['@timestamp'] || action.timestamp;
        allTransfers.push({
          account: to,
          received: amount,
          timestamp: tstamp
        });
        console.log(tstamp, " account: ", to, " => ", amount, " SEEDS", )
      }
    }

    // 3a) Sort recipients by total descending (for the top CSV)
    const sortedArray = Object.entries(recipientsMap)
      .map(([account, total]) => ({ account, total }))
      .sort((a, b) => b.total - a.total);

    // 3b) Sort the allTransfers array by timestamp ascending
    allTransfers.sort((a, b) => new Date(a.timestamp) - new Date(b.timestamp));

    // 4) Show top 10 in console
    console.log('\nTop 10 recipients by total SEEDS received:');
    console.table(sortedArray.slice(0, 10));

    // 5) Write CSV: top recipients (grant_receivers.csv)
    saveCsvTop(sortedArray, CSV_TOP);
    console.log(`Wrote CSV with top recipients => ${CSV_TOP}`);

    // 6) Write CSV: all transfers by time (all_recipients.csv)
    saveCsvAll(allTransfers, CSV_ALL);
    console.log(`Wrote CSV with all transfers => ${CSV_ALL}`);

    console.log('Done!');
  } catch (err) {
    console.error('Error in main:', err);
  }
}

/**
 * Use descending date-based pagination with 'before=...':
 * We keep fetching until we get 0 new actions in a response.
 */
async function fetchAllGiftSeedsTransfers() {
  let allActions = [];
  let keepFetching = true;
  let beforeTimestamp = ''; // The boundary for older actions

  while (keepFetching) {
    let url = new URL(HYPERION_ENDPOINT);
    url.searchParams.set('account', 'gift.seeds');   // only actions under gift.seeds
    url.searchParams.set('act.name', 'transfer');    // only 'transfer' action
    url.searchParams.set('sort', 'desc');            // newest -> oldest
    url.searchParams.set('limit', LIMIT.toString());

    if (beforeTimestamp) {
      url.searchParams.set('before', beforeTimestamp);
    }

    console.log(`Requesting ${url.toString()} ...`);
    const response = await fetch(url);
    if (!response.ok) {
      throw new Error(`HTTP error: ${response.status} - ${response.statusText}`);
    }

    const data = await response.json();
    const actions = data.actions || [];
    console.log(`  Got ${actions.length} actions this batch.`);

    if (actions.length === 0) {
      // No more data
      keepFetching = false;
      break;
    }

    // The last action is the oldest in this batch (descending order).
    const oldestAction = actions[actions.length - 1];
    const oldestTimestamp = oldestAction['@timestamp'] || oldestAction.timestamp;
    beforeTimestamp = oldestTimestamp; // next call will fetch older than this

    let newCount = 0;
    for (const action of actions) {
      const gseq = action.global_sequence;
      if (!seenGlobalSequences.has(gseq)) {
        seenGlobalSequences.add(gseq);
        allActions.push(action);
        newCount++;
      }
    }

    console.log(`  After dedup, added ${newCount} new actions.`);

    if (newCount === 0) {
      console.log('  All returned actions were duplicates => done.');
      keepFetching = false;
    }
  }

  return allActions;
}

/**
 * Save the top recipients summary to CSV with columns: total,account
 */
function saveCsvTop(arr, filename) {
  const header = 'total,account\n';
  const rows = arr
    .map(({ total, account }) => `${total},${account}`)
    .join('\n');
  fs.writeFileSync(filename, header + rows + '\n', 'utf-8');
}

/**
 * Save all individual transfers to CSV with columns: account,received,timestamp
 * sorted by time ascending
 */
function saveCsvAll(allTransfers, filename) {
  const header = 'account,received,timestamp\n';
  const rows = allTransfers
    .map(({ account, received, timestamp }) => `${account},${received},${timestamp}`)
    .join('\n');
  fs.writeFileSync(filename, header + rows + '\n', 'utf-8');
}

// Run
main();