<h1 class="contract">reset</h1>

---
spec_version: "0.2.0"
title: Reset
summary: 'This action will reset all the tables'
icon: https://joinseeds.com/assets/images/logos/seeds-app-Icon_190626.png#604c0741207bdc82b3214d575bc75cf705fa9cbc0cfa7c7553269ee0c550fd35
---

{{$action.authorization.[0].actor}} resets all the tables within this contract.


<h1 class="contract">createpost</h1>

---
spec_version: "0.2.0"
title: Create post
summary: 'Create a new post'
icon: https://joinseeds.com/assets/images/logos/seeds-app-Icon_190626.png#604c0741207bdc82b3214d575bc75cf705fa9cbc0cfa7c7553269ee0c550fd35
---

{{$action.authorization.[0].actor}} creates a new post with the following backend_id: {{backend_id}}, url: {{url}} and body: {{body}}.


<h1 class="contract">createcomt</h1>

---
spec_version: "0.2.0"
title: Create comment
summary: 'Create a new comment'
icon: https://joinseeds.com/assets/images/logos/seeds-app-Icon_190626.png#604c0741207bdc82b3214d575bc75cf705fa9cbc0cfa7c7553269ee0c550fd35
---

{{$action.authorization.[0].actor}} creates a new comment on the post with id: {{post_id}}. The comment has the following backend_id: {{backend_id}}, url: {{url}} and body: {{body}}.


<h1 class="contract">upvotepost</h1>

---
spec_version: "0.2.0"
title: Up vote post
summary: 'Give points to the given post'
icon: https://joinseeds.com/assets/images/logos/seeds-app-Icon_190626.png#604c0741207bdc82b3214d575bc75cf705fa9cbc0cfa7c7553269ee0c550fd35
---

{{$action.authorization.[0].actor}} gives positive points to the post with id: {{id}}.


<h1 class="contract">upvotecomt</h1>

---
spec_version: "0.2.0"
title: Up vote comment
summary: 'Give points to the given post'
icon: https://joinseeds.com/assets/images/logos/seeds-app-Icon_190626.png#604c0741207bdc82b3214d575bc75cf705fa9cbc0cfa7c7553269ee0c550fd35
---

{{$action.authorization.[0].actor}} gives positive points to the comment with id: {{comment_id}} on the post {{post_id}}.


<h1 class="contract">downvotepost</h1>

---
spec_version: "0.2.0"
title: Down vote post
summary: 'Give negative points to the given post'
icon: https://joinseeds.com/assets/images/logos/seeds-app-Icon_190626.png#604c0741207bdc82b3214d575bc75cf705fa9cbc0cfa7c7553269ee0c550fd35
---

{{$action.authorization.[0].actor}} gives negative points to the post with id: {{id}}.


<h1 class="contract">downvotecomt</h1>

---
spec_version: "0.2.0"
title: Down vote post
summary: 'Give negative points to the given comment'
icon: https://joinseeds.com/assets/images/logos/seeds-app-Icon_190626.png#604c0741207bdc82b3214d575bc75cf705fa9cbc0cfa7c7553269ee0c550fd35
---

{{$action.authorization.[0].actor}} gives negative points to the comment with id: {{comment_id}} on the post {{post_id}}.


<h1 class="contract">onperiod</h1>

---
spec_version: "0.2.0"
title: On period
summary: 'Devaluate the forum reputation'
icon: https://joinseeds.com/assets/images/logos/seeds-app-Icon_190626.png#604c0741207bdc82b3214d575bc75cf705fa9cbc0cfa7c7553269ee0c550fd35
---

{{$action.authorization.[0].actor}} devaluates the reputation of all the forums within this contract.


<h1 class="contract">newday</h1>

---
spec_version: "0.2.0"
title: New day
summary: 'Drop all the vote power entries'
icon: https://joinseeds.com/assets/images/logos/seeds-app-Icon_190626.png#604c0741207bdc82b3214d575bc75cf705fa9cbc0cfa7c7553269ee0c550fd35
---

{{$action.authorization.[0].actor}} drops all the vote power entries within this contract.

