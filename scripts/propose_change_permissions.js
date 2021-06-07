



async function proposePermissions({ contractName, proposalName }) {
    console.log('propose deployment of the contract ' + contractName)
    console.log('proposal name is ' + proposalName)

    const chainInfo = await rpc.get_info()
    const headBlock = await rpc.get_block(chainInfo.last_irreversible_block_num)
    const chainId = chainInfo.chain_id
}