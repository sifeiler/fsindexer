// src/utils/queryBuilders.ts
import { tauriExecuteSearch } from "./tauri/TauriInvokes";
import {
  SearchQuery,
  RelationalSearchQuery,
  LogicalSearchQuery,
  RelationalOperator,
  LogicalOperator,
  ValueType,
  SimilarityParams,
  SearchResult
} from "../types/Search";

export function matchText(attribute: string, value: string): RelationalSearchQuery {
  return {
    operator: RelationalOperator.EQ,
    attribute_name: attribute,
    value: {
        value_type: ValueType.TEXT,
        value
    }
  };
}

export function matchSimilarImagePath(
  attribute: string,
  params: SimilarityParams,
  value: string
): RelationalSearchQuery {
  return {
    operator: RelationalOperator.SIMILAR,
    attribute_name: attribute,
    value: {
        value_type: ValueType.IMAGE_PATH,
        ...params,
        value
    },
  };
}

export function matchSimilarImageBase64(
  attribute: string,
  params: SimilarityParams,
  value: string
): RelationalSearchQuery {
  return {
    operator: RelationalOperator.SIMILAR,
    attribute_name: attribute,
    value: {
        value_type: ValueType.IMAGE_BASE64,
        ...params,
        value
    },
  };
}

export function matchSimilarImagePrompt(
  attribute: string,
  params: SimilarityParams,
  value: string
): RelationalSearchQuery {
  return {
    operator: RelationalOperator.SIMILAR,
    attribute_name: attribute,
    value: {
        value_type: ValueType.IMAGE_PROMPT,
        ...params,
        value
    },
  };
}

export function and(...sub_queries: SearchQuery[]): LogicalSearchQuery {
  return { operator: LogicalOperator.AND, sub_queries };
}

export function or(...sub_queries: SearchQuery[]): LogicalSearchQuery {
  return { operator: LogicalOperator.OR, sub_queries };
}

/* Uses all known searchable attributes and builds a logical OR query with the provided query string. */
export async function executeWideSearch(query: string) :Promise<SearchResult> {
    let image_params :SimilarityParams = {
        ef: 64,
        top_k: 5,
        threshold: 0.3,
        vector_dim: 512
    };

    let prompt_params :SimilarityParams = {
        ef: 64,
        top_k: 5,
        threshold: 0.72,
        vector_dim: 512
    };

    //we try to find anything by searching over all available attributes. Query can be a path of a local image, a prompt, but also a file_name
    let orQuery :LogicalSearchQuery = or(
        matchSimilarImagePath("vector", image_params, query),
        matchSimilarImagePrompt("vector", prompt_params, query),
        matchText("file_name", query)
    );

    return await tauriExecuteSearch(orQuery);
}

export async function executeSearch() {

}