<template>
  <main class="container">
    <SearchView />
  </main>
</template>

<script setup lang="ts">
  import { ref, reactive } from "vue";
  import SearchView from "./views/SearchView.vue";

  interface AppState {
  }

  const state = reactive<AppState>({
  });

</script>

<style>
.container {
  width: 100%;
  min-height: 100vh; /* Fallback for older browsers */
  min-height: 100dvh; /* Modern fix for mobile browser URL bars */

  margin: 0;
  display: flex;
  flex-direction: column;
  justify-content: center;
  text-align: center;
}
</style>