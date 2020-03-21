<h1 class="contract">reset</h1>

---
spec_version: "0.2.0"
title: Reset
summary: 'This action will reset all the tables'
icon: https://joinseeds.com/assets/images/logos/seeds-app-Icon_190626.png#604c0741207bdc82b3214d575bc75cf705fa9cbc0cfa7c7553269ee0c550fd35
---

{{$action.authorization.[0].actor}} resets all the tables within this contract.


<h1 class="contract">addmember</h1>

---
spec_version: "0.2.0"
title: Add member
summary: 'Add a new member'
icon: https://joinseeds.com/assets/images/logos/seeds-app-Icon_190626.png#604c0741207bdc82b3214d575bc75cf705fa9cbc0cfa7c7553269ee0c550fd35
---

{{$action.authorization.[0].actor}} adds the new member {{account}} to the organization {{organization}} with role {{role}}.


<h1 class="contract">removemember</h1>

---
spec_version: "0.2.0"
title: Remove member
summary: 'Remove an existing member'
icon: https://joinseeds.com/assets/images/logos/seeds-app-Icon_190626.png#604c0741207bdc82b3214d575bc75cf705fa9cbc0cfa7c7553269ee0c550fd35
---

{{$action.authorization.[0].actor}} removes the member {{account}} from the organization {{organization}}.


<h1 class="contract">changerole</h1>

---
spec_version: "0.2.0"
title: Change role
summary: 'Assign a new role'
icon: https://joinseeds.com/assets/images/logos/seeds-app-Icon_190626.png#604c0741207bdc82b3214d575bc75cf705fa9cbc0cfa7c7553269ee0c550fd35
---

{{$action.authorization.[0].actor}} assigns the role {{new_role}} to {{account}} in the organization {{organization}}.


<h1 class="contract">changeowner</h1>

---
spec_version: "0.2.0"
title: Change owner
summary: 'Set a new owner for the given organization'
icon: https://joinseeds.com/assets/images/logos/seeds-app-Icon_190626.png#604c0741207bdc82b3214d575bc75cf705fa9cbc0cfa7c7553269ee0c550fd35
---

{{$action.authorization.[0].actor}} gives to {{account}} the ownership of the organization {{organization}}.


<h1 class="contract">addregen</h1>

---
spec_version: "0.2.0"
title: Add regen points
summary: 'Give positive regen points to the given organization'
icon: https://joinseeds.com/assets/images/logos/seeds-app-Icon_190626.png#604c0741207bdc82b3214d575bc75cf705fa9cbc0cfa7c7553269ee0c550fd35
---

{{$action.authorization.[0].actor}} gives positive regen points to the organization {{organization}}.


<h1 class="contract">subregen</h1>

---
spec_version: "0.2.0"
title: Substract regen points
summary: 'Give negative regen points to the given organization'
icon: https://joinseeds.com/assets/images/logos/seeds-app-Icon_190626.png#604c0741207bdc82b3214d575bc75cf705fa9cbc0cfa7c7553269ee0c550fd35
---

{{$action.authorization.[0].actor}} gives negative regen points to the organization {{organization}}.


<h1 class="contract">create</h1>

---
spec_version: "0.2.0"
title: Create
summary: 'Create a new organization'
icon: https://joinseeds.com/assets/images/logos/seeds-app-Icon_190626.png#604c0741207bdc82b3214d575bc75cf705fa9cbc0cfa7c7553269ee0c550fd35
---

{{$action.authorization.[0].actor}} creates a new organization called {{orgname}}.


<h1 class="contract">destroy</h1>

---
spec_version: "0.2.0"
title: Destroy
summary: 'Destroy the given organization'
icon: https://joinseeds.com/assets/images/logos/seeds-app-Icon_190626.png#604c0741207bdc82b3214d575bc75cf705fa9cbc0cfa7c7553269ee0c550fd35
---

{{$action.authorization.[0].actor}} destroys its organization {{orgname}}.


<h1 class="contract">refund</h1>

---
spec_version: "0.2.0"
title: Refund
summary: 'Withdraw the given quantity'
icon: https://joinseeds.com/assets/images/logos/seeds-app-Icon_190626.png#604c0741207bdc82b3214d575bc75cf705fa9cbc0cfa7c7553269ee0c550fd35
---

{{$action.authorization.[0].actor}} withdraws {{quantity}} from this contract.



