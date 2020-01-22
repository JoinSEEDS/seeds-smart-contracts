<h1 class="contract">reset</h1>

---
spec_version: "0.2.0"
title: Reset
summary: 'This action will reset all the tables'
icon: https://joinseeds.com/assets/images/logos/seeds-app-Icon_190626.png#604c0741207bdc82b3214d575bc75cf705fa9cbc0cfa7c7553269ee0c550fd35
---

{{$action.authorization.[0].actor}} resets all the tables within this contract.


<h1 class="contract">execute</h1>

---
spec_version: "0.2.0"
title: Execute
summary: 'Execute pending functions'
icon: https://joinseeds.com/assets/images/logos/seeds-app-Icon_190626.png#604c0741207bdc82b3214d575bc75cf705fa9cbc0cfa7c7553269ee0c550fd35
---

{{$action.authorization.[0].actor}} executes the first pending function defined in this contract.


<h1 class="contract">configop</h1>

---
spec_version: "0.2.0"
title: Config operation
summary: 'Set a new or an existing operation'
icon: https://joinseeds.com/assets/images/logos/seeds-app-Icon_190626.png#604c0741207bdc82b3214d575bc75cf705fa9cbc0cfa7c7553269ee0c550fd35
---

{{$action.authorization.[0].actor}} creates or edits an operation with the values action: {{action}}, contract: {{contract}} and period: {{period}}.


<h1 class="contract">confirm</h1>

---
spec_version: "0.2.0"
title: Confirm action
summary: 'Set a new or an existing operation'
icon: https://joinseeds.com/assets/images/logos/seeds-app-Icon_190626.png#604c0741207bdc82b3214d575bc75cf705fa9cbc0cfa7c7553269ee0c550fd35
---

{{$action.authorization.[0].actor}} confirms the successful execution of the action {{operation}}.






