export enum LogicalOperator {
    AND = "AND",
    OR = "OR",
    NOT = "NOT"
}

export enum RelationalOperator {
    EQ = "EQ",
    SIMILAR = "SIMILAR"
}

export enum ValueType {
    IMAGE_PATH = "image_path",
    IMAGE_PROMPT = "image_prompt",
    IMAGE_BASE64 = "image_base64",
    VECTOR = "vector",
    TEXT = "text",
    INT64 = "int64"
}

export type SimilarityValue =
    { value_type: ValueType.VECTOR, value: number[] } |
    { value_type: ValueType.IMAGE_PATH, value: string } |
    { value_type: ValueType.IMAGE_BASE64, value: string } |
    { value_type: ValueType.IMAGE_PROMPT, value: string }

export type SimilarityParams = {
    ef: number,
    top_k: number,
    threshold: number,
    vector_dim: number
}

export type VectorSearchValue = SimilarityValue & SimilarityParams

export type ExactSearchValue = 
    { value_type: ValueType.TEXT, value: string } |
    { value_type: ValueType.INT64, value: number }

export interface LogicalSearchQuery {
    operator: LogicalOperator,
    sub_queries: SearchQuery[]
}

export type SearchValue = VectorSearchValue | ExactSearchValue;

export interface RelationalSearchQuery {
    operator: RelationalOperator,
    attribute_name: string,
    value: SearchValue
}

export type SearchQuery = RelationalSearchQuery | LogicalSearchQuery;

export interface SearchResultEntry {
    path: string,
    name: string
}

export interface SearchResult {
    results: SearchResultEntry[],
    count: number
}