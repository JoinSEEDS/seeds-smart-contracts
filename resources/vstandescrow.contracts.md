<h1 class="contract">reset</h1>
---
title: Reset
summary: 'This action will reset all the tables'
icon: https://joinseeds.com/assets/images/logos/seeds-app-Icon_190626.png#604c0741207bdc82b3214d575bc75cf705fa9cbc0cfa7c7553269ee0c550fd35
---

{{$action.authorization.[0].actor}} resets all the tables within this contract.


<h1 class="contract">create</h1>
---
title: Create
summary: 'Create a new escrow'
icon: https://joinseeds.com/assets/images/logos/seeds-app-Icon_190626.png#604c0741207bdc82b3214d575bc75cf705fa9cbc0cfa7c7553269ee0c550fd35
---

{{$action.authorization.[0].actor}} creates a new escrow with {{beneficiary}} as the beneficiary for a quantity of {{quantity}} and vesting date {{vesting_date}}.


<h1 class="contract">createescrow</h1>
---
title: Create
summary: 'Create a new escrow'
icon: https://joinseeds.com/assets/images/logos/seeds-app-Icon_190626.png#604c0741207bdc82b3214d575bc75cf705fa9cbc0cfa7c7553269ee0c550fd35
---

{{$action.authorization.[0].actor}} creates a new escrow with {{beneficiary_org}} as the beneficiary for a quantity of {{quantity}} and vesting date {{vesting_date}}.


<h1 class="contract">cancel</h1>
---
title: Cancel
summary: 'Cancel an existing escrow'
icon: https://joinseeds.com/assets/images/logos/seeds-app-Icon_190626.png#604c0741207bdc82b3214d575bc75cf705fa9cbc0cfa7c7553269ee0c550fd35
---

{{$action.authorization.[0].actor}} cancels the existing escrow with the beneficiary {{beneficiary}}.


<h1 class="contract">cancelescrow</h1>
---
title: Cancel
summary: 'Cancel an existing escrow'
icon: https://joinseeds.com/assets/images/logos/seeds-app-Icon_190626.png#604c0741207bdc82b3214d575bc75cf705fa9cbc0cfa7c7553269ee0c550fd35
---

{{$action.authorization.[0].actor}} cancels the existing escrow with the beneficiary {{beneficiary}}.


<h1 class="contract">claim</h1>
---
title: Claim
summary: 'Claim the funds granted to an user'
icon: https://joinseeds.com/assets/images/logos/seeds-app-Icon_190626.png#604c0741207bdc82b3214d575bc75cf705fa9cbc0cfa7c7553269ee0c550fd35
---

{{$action.authorization.[0].actor}} claims all the funds available in the escrows with {{beneficiary}} as the beneficiary.


<h1 class="contract">withdraw</h1>
---
title: Withdraw
summary: 'Withdraw a given amount of tokens'
icon: https://joinseeds.com/assets/images/logos/seeds-app-Icon_190626.png#604c0741207bdc82b3214d575bc75cf705fa9cbc0cfa7c7553269ee0c550fd35
---

{{$action.authorization.[0].actor}} withdraws {{quantity}} from its balance in this contract.

